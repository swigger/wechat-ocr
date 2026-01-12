use axum::{
    body::Bytes,
    extract::{DefaultBodyLimit, Multipart, State},
    http::StatusCode,
    response::{IntoResponse, Json},
    routing::post,
    Router,
};
use clap::Parser;
use libloading::{Library, Symbol};
use serde::{Deserialize, Serialize};
use std::{
    ffi::{c_char, CStr, CString},
    path::PathBuf,
    sync::Arc,
};
use tempfile::NamedTempFile;
use tokio::sync::Semaphore;
use tower::limit::ConcurrencyLimitLayer;
use tracing::info;

#[derive(Parser, Debug)]
#[command(name = "wcocr-server")]
#[command(about = "WeChat OCR HTTP Server")]
struct Args {
    /// Path to libwcocr.so (or set WCOCR_LIB_PATH env var)
    #[arg(long, env = "WCOCR_LIB_PATH")]
    lib_path: Option<String>,

    /// OCR executable path
    #[arg(long, default_value = "/opt/wechat/wxocr")]
    ocr_exe: String,

    /// WeChat directory path
    #[arg(long, default_value = "/opt/wechat")]
    wechat_dir: String,

    /// HTTP server port
    #[arg(short, long, default_value = "8080")]
    port: u16,

    /// Maximum concurrent requests
    #[arg(short, long, default_value = "5")]
    max_concurrent: usize,

    /// Request timeout in seconds
    #[arg(short, long, default_value = "30")]
    timeout: u64,
}

fn find_library(explicit_path: Option<&str>) -> Result<PathBuf, String> {
    // 1. Explicit path from argument or env var
    if let Some(p) = explicit_path {
        let path = PathBuf::from(p);
        if path.exists() {
            return Ok(path);
        }
        return Err(format!("Specified library not found: {}", p));
    }

    // 2. Try current executable's directory
    if let Ok(exe_path) = std::env::current_exe() {
        if let Some(exe_dir) = exe_path.parent() {
            let lib_path = exe_dir.join("libwcocr.so");
            if lib_path.exists() {
                return Ok(lib_path);
            }
        }
    }

    // 3. Try current working directory
    let cwd_lib = PathBuf::from("libwcocr.so");
    if cwd_lib.exists() {
        return Ok(cwd_lib);
    }

    // 4. Try system library name (let dlopen search LD_LIBRARY_PATH, /usr/lib, etc.)
    Ok(PathBuf::from("libwcocr.so"))
}

type WechatOcrFn = unsafe extern "C" fn(
    ocr_exe: *const c_char,
    wechat_dir: *const c_char,
    imgfn: *const c_char,
    set_res: Option<extern "C" fn(*const c_char)>,
) -> bool;

type StopOcrFn = unsafe extern "C" fn();

struct OcrLib {
    _lib: Library,
    wechat_ocr: Symbol<'static, WechatOcrFn>,
    #[allow(dead_code)]
    stop_ocr: Symbol<'static, StopOcrFn>,
}

unsafe impl Send for OcrLib {}
unsafe impl Sync for OcrLib {}

struct AppState {
    ocr_lib: OcrLib,
    ocr_exe: CString,
    wechat_dir: CString,
    semaphore: Semaphore,
    timeout_secs: u64,
}

#[derive(Serialize, Deserialize)]
struct OcrResponse {
    errcode: i32,
    imgpath: String,
    width: i32,
    height: i32,
    ocr_response: Vec<TextBlock>,
}

#[derive(Serialize, Deserialize)]
struct TextBlock {
    left: f32,
    top: f32,
    right: f32,
    bottom: f32,
    rate: f32,
    text: String,
}

#[derive(Serialize)]
struct ErrorResponse {
    error: String,
}

thread_local! {
    static OCR_RESULT: std::cell::RefCell<Option<String>> = const { std::cell::RefCell::new(None) };
}

extern "C" fn set_result_callback(result: *const c_char) {
    if !result.is_null() {
        let c_str = unsafe { CStr::from_ptr(result) };
        if let Ok(s) = c_str.to_str() {
            OCR_RESULT.with(|r| {
                *r.borrow_mut() = Some(s.to_string());
            });
        }
    }
}

fn do_ocr(state: &AppState, img_path: &str) -> Result<String, String> {
    let img_path_c = CString::new(img_path).map_err(|e| e.to_string())?;

    OCR_RESULT.with(|r| {
        *r.borrow_mut() = None;
    });

    let success = unsafe {
        (state.ocr_lib.wechat_ocr)(
            state.ocr_exe.as_ptr(),
            state.wechat_dir.as_ptr(),
            img_path_c.as_ptr(),
            Some(set_result_callback),
        )
    };

    if !success {
        return Err("OCR processing failed".to_string());
    }

    OCR_RESULT.with(|r| {
        r.borrow().clone().ok_or_else(|| "No OCR result returned".to_string())
    })
}

async fn ocr_handler(
    State(state): State<Arc<AppState>>,
    mut multipart: Multipart,
) -> impl IntoResponse {
    let _permit = match tokio::time::timeout(
        std::time::Duration::from_secs(state.timeout_secs),
        state.semaphore.acquire(),
    )
    .await
    {
        Ok(Ok(permit)) => permit,
        Ok(Err(_)) => {
            return (
                StatusCode::SERVICE_UNAVAILABLE,
                Json(ErrorResponse {
                    error: "Service unavailable".to_string(),
                }),
            )
                .into_response();
        }
        Err(_) => {
            return (
                StatusCode::REQUEST_TIMEOUT,
                Json(ErrorResponse {
                    error: "Request timeout waiting for available slot".to_string(),
                }),
            )
                .into_response();
        }
    };

    let image_data: Option<Bytes> = match multipart.next_field().await {
        Ok(Some(field)) => match field.bytes().await {
            Ok(data) => Some(data),
            Err(e) => {
                return (
                    StatusCode::BAD_REQUEST,
                    Json(ErrorResponse {
                        error: format!("Failed to read image data: {}", e),
                    }),
                )
                    .into_response();
            }
        },
        Ok(None) => None,
        Err(e) => {
            return (
                StatusCode::BAD_REQUEST,
                Json(ErrorResponse {
                    error: format!("Failed to parse multipart: {}", e),
                }),
            )
                .into_response();
        }
    };

    let image_data = match image_data {
        Some(data) => data,
        None => {
            return (
                StatusCode::BAD_REQUEST,
                Json(ErrorResponse {
                    error: "No image provided".to_string(),
                }),
            )
                .into_response();
        }
    };

    let temp_file = match NamedTempFile::new() {
        Ok(f) => f,
        Err(e) => {
            return (
                StatusCode::INTERNAL_SERVER_ERROR,
                Json(ErrorResponse {
                    error: format!("Failed to create temp file: {}", e),
                }),
            )
                .into_response();
        }
    };

    if let Err(e) = std::fs::write(temp_file.path(), &image_data) {
        return (
            StatusCode::INTERNAL_SERVER_ERROR,
            Json(ErrorResponse {
                error: format!("Failed to write temp file: {}", e),
            }),
        )
            .into_response();
    }

    let img_path = temp_file.path().to_string_lossy().to_string();
    let state_clone = state.clone();

    let result = tokio::time::timeout(
        std::time::Duration::from_secs(state.timeout_secs),
        tokio::task::spawn_blocking(move || do_ocr(&state_clone, &img_path)),
    )
    .await;

    match result {
        Ok(Ok(Ok(json_str))) => match serde_json::from_str::<serde_json::Value>(&json_str) {
            Ok(json) => (StatusCode::OK, Json(json)).into_response(),
            Err(e) => (
                StatusCode::INTERNAL_SERVER_ERROR,
                Json(ErrorResponse {
                    error: format!("Failed to parse OCR result: {}", e),
                }),
            )
                .into_response(),
        },
        Ok(Ok(Err(e))) => (
            StatusCode::INTERNAL_SERVER_ERROR,
            Json(ErrorResponse { error: e }),
        )
            .into_response(),
        Ok(Err(e)) => (
            StatusCode::INTERNAL_SERVER_ERROR,
            Json(ErrorResponse {
                error: format!("Task execution error: {}", e),
            }),
        )
            .into_response(),
        Err(_) => (
            StatusCode::REQUEST_TIMEOUT,
            Json(ErrorResponse {
                error: "OCR processing timeout".to_string(),
            }),
        )
            .into_response(),
    }
}

async fn health_handler() -> impl IntoResponse {
    (StatusCode::OK, "OK")
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt::init();

    let args = Args::parse();

    let lib_path = find_library(args.lib_path.as_deref())?;
    info!("Loading OCR library from {:?}", lib_path);

    let lib = unsafe { Library::new(&lib_path) }
        .map_err(|e| format!("Failed to load libwcocr.so from {:?}: {}", lib_path, e))?;

    let wechat_ocr: Symbol<WechatOcrFn> =
        unsafe { std::mem::transmute(lib.get::<WechatOcrFn>(b"wechat_ocr\0")?) };
    let stop_ocr: Symbol<StopOcrFn> =
        unsafe { std::mem::transmute(lib.get::<StopOcrFn>(b"stop_ocr\0")?) };

    let ocr_lib = OcrLib {
        _lib: lib,
        wechat_ocr,
        stop_ocr,
    };

    let ocr_exe = CString::new(args.ocr_exe.clone())?;
    let wechat_dir = CString::new(args.wechat_dir.clone())?;

    info!(
        "Initializing OCR engine with exe={}, dir={}",
        args.ocr_exe, args.wechat_dir
    );

    // Initialize OCR engine by calling with empty image path
    let empty_path = CString::new("")?;
    let init_success = unsafe {
        (ocr_lib.wechat_ocr)(
            ocr_exe.as_ptr(),
            wechat_dir.as_ptr(),
            empty_path.as_ptr(),
            None,
        )
    };

    if !init_success {
        return Err("Failed to initialize OCR engine. Please check ocr_exe and wechat_dir paths.".into());
    }

    info!("OCR engine initialized successfully");

    let state = Arc::new(AppState {
        ocr_lib,
        ocr_exe,
        wechat_dir,
        semaphore: Semaphore::new(args.max_concurrent),
        timeout_secs: args.timeout,
    });

    let app = Router::new()
        .route("/ocr", post(ocr_handler))
        .route("/health", post(health_handler))
        .layer(DefaultBodyLimit::max(50 * 1024 * 1024)) // 50MB max
        .layer(ConcurrencyLimitLayer::new(args.max_concurrent))
        .with_state(state);

    let addr = format!("0.0.0.0:{}", args.port);
    info!(
        "Starting server on {} with max {} concurrent requests",
        addr, args.max_concurrent
    );

    let listener = tokio::net::TcpListener::bind(&addr).await?;
    axum::serve(listener, app).await?;

    Ok(())
}
