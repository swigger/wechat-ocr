#include "stdafx.h"
#include "wechatocr.h"
#include <google/protobuf/stubs/common.h>

static string json_encode(const string& input) {
	string output;
	output.reserve(input.size() + 2);
	for (auto c : input) {
		switch (c) {
		case '"': output += "\\\"";	break;
		case '\\': output += "\\\\"; break;
		case '\n': output += "\\n"; break;
		case '\r': output += "\\r";	break;
		case '\t': output += "\\t";	break;
		default:
			if (c>=0 && c < 0x20) {
				char buf[16];
				snprintf(buf, sizeof(buf), "\\u%04x", c);
				output += buf;
			} else {
				output.push_back(c);
			}
		}
	}
	return output;
}

static std::unique_ptr<CWeChatOCR> g_instance;

extern "C" EXPORTED_API
bool wechat_ocr(LPCTSTR ocr_exe, LPCTSTR wechat_dir, const char * imgfn, void(*set_res)(const char * dt))
{
	if (!g_instance) {
		if (!ocr_exe || !wechat_dir || !*ocr_exe || !*wechat_dir)
			return false;
		auto ocr = std::make_unique<CWeChatOCR>(ocr_exe, wechat_dir);
		if (!ocr->wait_connection(5000)) {
			return false;
		}
		g_instance = std::move(ocr);
	}

	string json;
	if (!imgfn || !*imgfn) {
		json = "{\"errcode\":0}";
	} else {
		CWeChatOCR::result_t res;
		if (!g_instance->doOCR(imgfn, &res))
			return false;
		json += "{";
		json += "\"errcode\":" + std::to_string(res.errcode) + ",";
		json += "\"imgpath\":\"" + json_encode(res.imgpath) + "\",";
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
			json += "\"text\":\"" + json_encode(blk.text) + "\"";
			json += "},";
		}
		if (json.back() == ',') {
			json.pop_back();
		}
		json += "]}";
	}
	if (set_res) {
		set_res(json.c_str());
	}
	return true;
}

extern "C" EXPORTED_API
void stop_ocr() {
	g_instance.reset();
}
