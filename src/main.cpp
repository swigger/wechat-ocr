#include "stdafx.h"
#include "wechatocr.h"
#include <google/protobuf/stubs/common.h>

// dllmain
int APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

extern "C" __declspec(dllexport)
bool wechat_ocr(const wchar_t* ocr_exe, const wchar_t * wechat_dir, const char * imgfn, void(*set_res)(const char * dt))
{
	CWeChatOCR wechat_ocr(ocr_exe, wechat_dir);
	if (!wechat_ocr.wait_connection(5000)) {
		return false;
	}
	CWeChatOCR::result_t res;
	wechat_ocr.doOCR(imgfn, &res);
	string json;
	json += "{";
	json += "\"errcode\":" + std::to_string(res.errcode) + ",";
	json += "\"imgpath\":\"" + res.imgpath + "\",";
	json += "\"width\":" + std::to_string(res.width) + ",";
	json += "\"height\":" + std::to_string(res.height) + ",";
	json += "\"ocr_response\":[";
	for (auto& blk : res.ocr_response) {
		json += "{";
		json += "\"left\":" + std::to_string(blk.left) + ",";
		json += "\"top\":" + std::to_string(blk.top) + ",";
		json += "\"right\":" + std::to_string(blk.right) + ",";
		json += "\"bottom\":" + std::to_string(blk.bottom) + ",";
		json += "\"rate\":" + std::to_string(blk.rate) + ",";
		json += "\"text\":\"" + blk.text + "\"";
		json += "},";
	}
	if (!res.ocr_response.empty()) {
		json.pop_back();
	}
	json += "]}";
	if (set_res) {
		set_res(json.c_str());
	}
	return true;
}

#ifdef _DEBUG
// 定义这个函数仅方便使用regsvr32.exe调试本DLL, 使用环境变量WECHATOCR_EXE和WECHAT_DIR以及TEST_PNG传入调试参数
extern "C" __declspec(dllexport)
HRESULT DllRegisterServer(void)
{
	if (AllocConsole()) {
		(void)freopen("CONOUT$", "w", stdout);
		(void)freopen("CONOUT$", "w", stderr);
		// disalbe stdout cache.
		setvbuf(stdout, NULL, _IONBF, 0);
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);
	}

	// printf("Protobuf version: %u\n", GOOGLE_PROTOBUF_VERSION);
	// printf("tencent protobuf version string: %s\n", google::protobuf::internal::VersionString(2005000).c_str());
	string msg;

	CWeChatOCR wechat_ocr(_wgetenv(L"WECHATOCR_EXE"), _wgetenv(L"WECHAT_DIR"));
	if (!wechat_ocr.wait_connection(5000)) {
		return E_FAIL;
	}
	wechat_ocr.doOCR(getenv("TEST_PNG"), nullptr);
	// MessageBoxW(NULL, L"注册成功", L"提示", MB_OK);
	wechat_ocr.wait_done(-1);
	return S_OK;
}
#endif
