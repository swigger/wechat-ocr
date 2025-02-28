#include "stdafx.h"
#include "wechatocr.h"
#include "ocr_wx3.pb.h"
#include "ocr_wx4.pb.h"
#include <filesystem>

namespace fs = std::filesystem;

namespace {
	bool is_text_utf8(const char* sin, size_t len) {
		const unsigned char* s = (const unsigned char*)sin;
		const unsigned char* end = s + len;
		while (s < end) {
			if (*s < 0x80) {
				++s;
			} else if (*s < 0xC0) {
				return false;
			} else if (*s < 0xE0) {
				if (s + 1 >= end || (s[1] & 0xC0) != 0x80)
					return false;
				s += 2;
			} else if (*s < 0xF0) {
				if (s + 2 >= end || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80)
					return false;
				s += 3;
			} else if (*s < 0xF8) {
				if (s + 3 >= end || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80)
					return false;
				s += 4;
			} else {
				return false;
			}
		}
		return true;
	}
}

CWeChatOCR::CWeChatOCR(LPCTSTR exe0, LPCTSTR wcdir0)
{
	tstring exe = exe0;
	tstring wcdir = wcdir0;
#ifdef _WIN32
	// convert / to '\\'
	std::replace(wcdir.begin(), wcdir.end(), L'/', L'\\');
	std::replace(exe.begin(), exe.end(), L'/', L'\\');
#endif
	if (!fs::is_regular_file(exe) || !fs::is_directory(wcdir)) {
		// 传入的ocr.exe / wcdir 路径无效
		m_state = MJC_FAILED;
		return;
	}

	if (Init(wcdir.c_str()))
	{
		m_args["no-sandbox"] = WIN_POSIX(L"", "");

#ifdef _WIN32
		m_args["user-lib-dir"] = wcdir;
		// 测试 wc4.0, 改成 dll了
		auto fn_ext = exe.substr(exe.size() - 4);
		if (wcsicmp(fn_ext.c_str(), L".dll") == 0) {
			auto exe2 = wcdir;
			if (exe2.back() != '\\') exe2.push_back('\\');
			exe2 += L"..\\weixin.exe";
			auto sep_pos = exe.rfind('\\');
			tstring app_path = exe.substr(0, sep_pos);
			tstring app_name = exe.substr(sep_pos + 1);
			if (size_t dot_pos = app_name.rfind('.'); dot_pos != std::wstring::npos) {
				app_name.resize(dot_pos);
			}
			m_args["type"] = std::move(app_name);
			m_args["app-path"] = std::move(app_path);
			exe = std::move(exe2);
			m_version = 400;
		}
#else
		m_version = 400;  // Linux 强制使用 4.0 版本
#endif
		if (!Start(exe.c_str()))
		{
			std::lock_guard<std::mutex> lock(m_mutex_state);
			m_state = MJC_FAILED;
			m_cv_state.notify_all();
		}
	}
}

#define OCR_MAX_TASK_ID INT_MAX
bool CWeChatOCR::doOCR(crefstr imgpath0, result_t* res)
{
	if (!wait_connection(2000))
		return false;

	// wx4 中，图片路径必须是绝对路径，否则会失败
	string imgpath;
#ifndef _WIN32
	do {
		auto u8s = std::filesystem::weakly_canonical(imgpath0).u8string();
		imgpath = std::string(u8s.begin(), u8s.end());
	} while(0);
#else
	do {
		std::wstring wtmp;
		wtmp.resize(imgpath0.length() + 2);
		DWORD cp = is_text_utf8(imgpath0.c_str(), imgpath0.length()) ? CP_UTF8 : CP_ACP;
		int len = MultiByteToWideChar(cp, 0, imgpath0.c_str(), (int)imgpath0.length(), &wtmp[0], (int)wtmp.size());
		wtmp.resize(len > 0 ? len : 0);
		auto patho = std::filesystem::weakly_canonical(wtmp);
		if (!patho.empty()) {
			auto u8s = patho.u8string();
			imgpath = std::string(u8s.begin(), u8s.end());
		} else {
			if (res) {
				res->errcode = -2;
				res->imgpath = imgpath0;
			}
			return false;
		}
	} while (0);
#endif

	int found_id = -1;
	m_mutex.lock();
	// XXX: task_id本身可以是任何正整形数，但是这里要不要限制同时并发任务数呢？
	// 由于1号task_id已经被用于hello消息，所以从2开始。当然1号也不是不能重用，但为了安全起见不用了，也不差这一个task_id。
	for (uint32_t i = 2; i <= OCR_MAX_TASK_ID; ++i)
	{
		if (m_idpath.insert(std::make_pair(i, std::pair<string,result_t*>(imgpath0,res))).second)
		{
			found_id = i;
			break;
		}
	}
	m_mutex.unlock();
	if (found_id < 0)  // too many tasks.
		return false;

	std::string data_;
	uint32_t reqid = 0;
	//创建OcrRequest的pb数据
	if (m_version == 300) {
		wx3::OcrRequest ocr_request;
		ocr_request.set_type(0);
		ocr_request.set_task_id(found_id);
		auto pp = new wx3::OcrInputBuffer;
		pp->set_pic_path(imgpath);
		ocr_request.set_allocated_input(pp);
		ocr_request.SerializeToString(&data_);
		// 这个reqid根本没有用到，填啥都行
		reqid = mmmojo::RequestIdOCR3::OCRPush;
	}
	else if (m_version == 400) {
		wx4::ParseOCRReqMessage req;
		req.set_task_id(found_id);
		req.set_pic_path(imgpath);
		auto rt = new wx4::ReqType;
		// 我也不知道这仨是干啥的...
		rt->set_t1(true);
		rt->set_t2(true);
		rt->set_t3(false);
		req.set_allocated_rt(rt);
		req.SerializeToString(&data_);
		reqid = mmmojo::RequestIdOCR4::REQ_OCR;
	}
	// fprintf(stderr, "Sending Request...\n");
	bool bx = SendPbSerializedData(data_.data(), data_.size(), MMMojoInfoMethod::kMMPush, false, reqid);
	if (bx && res) {
		// wait for result.
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv_idpath.wait(lock, [this, found_id] {return m_idpath.find(found_id) == m_idpath.end(); });
	}
	return bx;
}

void CWeChatOCR::ReadOnPush(uint32_t request_id, std::span<std::byte> request_info)
{
	if (request_info.empty())
		return;
	auto init_done = [this](bool init_ok) {
		std::lock_guard<std::mutex> lock(m_mutex_state);
		if (m_state == MJC_PENDING || m_state == MJC_CONNECTED)
		{
			m_state = init_ok ? STATE_INITED : MJC_FAILED;
			m_cv_state.notify_all();
		}
	};
	auto ocr_copy = [this](const auto & ores, int ec, result_t& res) {
		res.width = ores.img_width();
		res.height = ores.img_height();
		res.errcode = ec;
		res.ocr_response.reserve(ores.lines_size());
		for (int i = 0, mi = ores.lines_size(); i < mi; ++i) {
			text_block_t tb;
			const auto& single_result = ores.lines(i);
			tb.left = single_result.left();
			tb.top = single_result.top();
			tb.right = single_result.right();
			tb.bottom = single_result.bottom();
			tb.rate = single_result.rate();
			tb.text = single_result.text();
			// printf("{%.1f,%.1f,%.1f,%.1f}: \"%s\", rate=%.3f\n", tb.left, tb.top, tb.right, tb.bottom, tb.text.c_str(), tb.rate);
			res.ocr_response.push_back(std::move(tb));
		}
	};
	auto ocr_done = [this](uint64_t task_id, result_t & res) {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			auto it = m_idpath.find(task_id);
			if (it != m_idpath.end())
			{
				res.imgpath = std::move(it->second.first);
				auto wres = it->second.second;
				if (wres) *wres = res;
			}
		}
		OnOCRResult(res);
		std::lock_guard<std::mutex> lock(m_mutex);
		m_idpath.erase(task_id);
		m_cv_idpath.notify_all();
	};

	result_t res;
	if (m_version >= 400) {
		if (request_id == mmmojo::RequestIdOCR4::HAND_SHAKE) {
			wx4::OCRSupportMessage msg;
			if (!msg.ParseFromArray(request_info.data(), (int)request_info.size()))
				return;
			bool supported = msg.has_supported() && msg.supported();
			if (!supported) {
				fprintf(stderr, "OCR4: OCR not supported\n");
			}
			init_done(supported);
		}
		else if (request_id == mmmojo::RequestIdOCR4::RESP_OCR) {
			wx4::ParseOCRRespMessage resp;
			if (!resp.ParseFromArray(request_info.data(), (int)request_info.size()))
				return;
			if (resp.has_res()) {
				ocr_copy(resp.res(), resp.err_code(), res);
			} else {
				res.errcode = resp.has_err_code() ? resp.err_code() : -1;
			}
			ocr_done(resp.task_id(), res);
		}
		return;
	} else if (m_version >= 300) {
		if (request_id == mmmojo::RequestIdOCR3::OCRPush) {
			wx3::OcrRespond resp;
			resp.ParseFromArray(request_info.data(), (int)request_info.size());
			switch (resp.type()) {
			case 1: // init response, with taskid=1, ec=0
				do {
					auto ec = resp.err_code();
					if (ec != 0) fprintf(stderr, "WX3: init failed, ec=%d\n", ec);
					init_done(ec == 0);
				} while (0);
				break;
			case 0:
				if (resp.has_ocr_result()) {
					ocr_copy(resp.ocr_result(), resp.err_code(), res);
				} else {
					res.errcode = resp.has_err_code() ? resp.err_code() : -1;
				}
				ocr_done(resp.task_id(), res);
				break;
			default:
				fprintf(stderr, "WX3: got responce with type %d\n", resp.type());
				break;
			}
		}
	}
}

bool CWeChatOCR::wait_connection(int timeout)
{
	if (m_state != MJC_PENDING) {
		return m_state >= STATE_INITED;
	}
	auto checker = [this] {return m_state >= STATE_INITED || m_state == MJC_FAILED; };
	if (timeout < 0) {
		std::unique_lock<std::mutex> lock(m_mutex_state);
		m_cv_state.wait(lock, checker);
	} else {
		std::unique_lock<std::mutex> lock(m_mutex_state);
		if (!m_cv_state.wait_for(lock, std::chrono::milliseconds(timeout), checker))
		{
			return false;
		}
	}
	return m_state >= STATE_INITED;
}

bool CWeChatOCR::wait_done(int timeout)
{
	if (!wait_connection(timeout))
		return false;

	std::unique_lock<std::mutex> lock(m_mutex);
	if (timeout < 0)
		return m_cv_idpath.wait(lock, [this] {return m_idpath.empty(); }), true;
	else
		return m_cv_idpath.wait_for(lock, std::chrono::milliseconds(timeout), [this] {return m_idpath.empty(); });
}

void CWeChatOCR::OnOCRResult(result_t& res)
{
	// do nothing.
}
