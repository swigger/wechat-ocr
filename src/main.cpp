#include "stdafx.h"
#include "wechatocr.h"
#include <locale.h>

#if defined(TEST_CLI)
class CGotOCR : public CWeChatOCR
{
public:
	CGotOCR(LPCTSTR exe, LPCTSTR wcdir)
	    : CWeChatOCR(exe, wcdir)
	{}
	void OnOCRResult(result_t& ocr_response) override
	{
		printf("OCR errcode=%d\n", ocr_response.errcode);
		for (auto& text : ocr_response.ocr_response) {
			printf("[%.2f,%.2f,%.2f,%.2f] r=%.3f %s\n", text.left, text.top, text.right, text.bottom,
			    text.rate, text.text.c_str());
		}
	}
};

int main(int argc, char* argv[])
{
#ifdef _WIN32
	SetConsoleCP(CP_UTF8);
	SetConsoleOutputCP(CP_UTF8);
	setlocale(LC_ALL, "en_US.UTF-8");
#endif
	if (argc != 4) {
		fprintf(stderr, "Usage: %s <wechatocr_exe> <wechat_dir> <test_png>\n", argv[0]);
		return -1;
	}

	string msg;
	CGotOCR ocr(util::to_tstr(argv[1]).c_str(), util::to_tstr(argv[2]).c_str());
	if (!ocr.wait_connection(5000)) {
		fprintf(stderr, "wechat_ocr.wait_connection failed\n");
		return -1;
	}
	ocr.doOCR(argv[3], nullptr);
	ocr.wait_done(-1);
	fprintf(stderr, "debug play ocr DONE!\n");
	return 0;
}
#elif defined(_WIN32)
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

# ifdef _DEBUG
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
		fprintf(stderr, "wechat_ocr.wait_connection failed\n");
		return E_FAIL;
	}
	wechat_ocr.doOCR(getenv("TEST_PNG"), nullptr);
	wechat_ocr.wait_done(-1);
	fprintf(stderr, "debug play ocr DONE!\n");
	return S_OK;
}
# endif
#endif
