#include "stdafx.h"
#include "wechatocr.h"
#include "ocr_protobuf.pb.h"
#include "mmmojo.h"

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

CWeChatOCR::CWeChatOCR(LPCWSTR exe, LPCWSTR wcdir)
{
	m_exe = exe;
	m_wcdir = wcdir;
	DWORD attr1 = GetFileAttributesW(exe);
	if (attr1 == INVALID_FILE_ATTRIBUTES || (attr1 & FILE_ATTRIBUTE_DIRECTORY) != 0)
	{
		// 传入的ocr.exe路径无效
		m_state = STATE_INVALID;
		return;
	}
	DWORD attr2 = GetFileAttributesW(wcdir);
	if (attr2 == INVALID_FILE_ATTRIBUTES || (attr2 & FILE_ATTRIBUTE_DIRECTORY) == 0)
	{
		// 传入的微信目录无效
		m_state = STATE_INVALID;
		return;
	}

	if (Init(m_wcdir.c_str()))
	{
		m_args["user-lib-dir"] = m_wcdir;
		// in initializaion, task id 1 is used for helo msg.
		m_idpath.insert(std::make_pair(1, std::pair<string, result_t*>()));
		if (Start(m_exe.c_str()))
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_state == STATE_INVALID)
				m_state = STATE_PENDING;
		}
	}
}

#define OCR_MAX_TASK_ID INT_MAX
bool CWeChatOCR::doOCR(crefstr imgpath0, result_t* res)
{
	if (m_state == STATE_INVALID || (m_state == STATE_PENDING && !wait_connection(2000)))
		return false;

	// 为了更好的中文支持，这里检查imgpath是否是utf8编码的，如果是GBK，需要转换为utf8编码。
	// TODO: 其实不应该写在这里，应该是调用者确保传入的imgpath是utf8编码的。
	string tmp;
	const string * imgpath = &imgpath0;
	if (!is_text_utf8(imgpath0.c_str(), imgpath0.length())) {
		wstring wimgpath;
		wimgpath.resize(imgpath0.size() + 10);
		int len = MultiByteToWideChar(CP_ACP, 0, imgpath0.c_str(), (int)imgpath0.length(), &wimgpath[0], (int)wimgpath.size());
		if (len > 0) {
			tmp.resize(len * 3 + 10);
			len = WideCharToMultiByte(CP_UTF8, 0, wimgpath.c_str(), len, &tmp[0], (int)tmp.size(), 0, 0);
			if (len > 0) {
				tmp.resize(len);
				imgpath = &tmp;
			}
		}
	}

	int found_id = -1;
	m_mutex.lock();
	// TODO: task_id本身可以是任何正整形数，但是这里要不要限制同时并发任务数呢？
	for (uint32_t i = 1; i <= OCR_MAX_TASK_ID; ++i)
	{
		if (m_idpath.insert(std::make_pair(i, std::pair<string,result_t*>(*imgpath,res))).second)
		{
			found_id = i;
			break;
		}
	}
	m_mutex.unlock();
	if (found_id < 0)  // too many tasks.
		return false;

	//创建OcrRequest的pb数据
	ocr_protobuf::OcrRequest ocr_request;
	ocr_request.set_unknow(0);
	ocr_request.set_task_id(found_id);
	auto pp = new ocr_protobuf::OcrRequest::OcrInputBuffer;
	pp->set_pic_path(*imgpath);
	ocr_request.set_allocated_input(pp);

	std::string data_;
	ocr_request.SerializeToString(&data_);

	bool bx = SendPbSerializedData(data_.data(), data_.size(), MMMojoInfoMethod::kMMPush, false, mmmojo::RequestIdOCR::OCRPush);
	if (bx && res) {
		// wait for result.
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv_idpath.wait(lock, [this, found_id] {return m_idpath.find(found_id) == m_idpath.end(); });
	}
	return bx;
}

void CWeChatOCR::ReadOnPush(uint32_t request_id, const void* request_info)
{
	uint32_t pb_size;
	const void* pb_data = GetMMMojoReadInfoRequest(request_info, &pb_size);
	if (pb_data) {
		ocr_protobuf::OcrRespond ocr_response;
		ocr_response.ParseFromArray(pb_data, pb_size);
		uint64_t task_id = ocr_response.task_id();
		uint32_t type = ocr_response.type();
		auto ec = ocr_response.err_code();
		if (type == 1) {
			// init success.
			std::lock_guard<std::mutex> lock(m_mutex);
			m_idpath.erase(task_id);
			m_cv_idpath.notify_all();
			if (m_state == STATE_PENDING)
			{
				m_state = STATE_VALID;
				m_cv_state.notify_all();
			}
		}
		if (type == 0)
		{
			// ocr result.
			result_t res, *wres=0;
			res.errcode = ec;
			res.width = 0;
			res.height = 0;
			if (ocr_response.has_ocr_result()) {
				const auto& ocr_result = ocr_response.ocr_result();
				res.width = ocr_result.img_width();
				res.height = ocr_result.img_height();

				for (int i = 0, mi = ocr_result.lines_size(); i < mi; ++i) {
					text_block_t tb;
					const auto& single_result = ocr_result.lines(i);
					tb.left = single_result.left();
					tb.top = single_result.top();
					tb.right = single_result.right();
					tb.bottom = single_result.bottom();
					tb.rate = single_result.rate();
					tb.text = single_result.text();
					// printf("{%.1f,%.1f,%.1f,%.1f}: \"%s\", rate=%.3f\n", tb.left, tb.top, tb.right, tb.bottom, tb.text.c_str(), tb.rate);
					res.ocr_response.push_back(std::move(tb));
				}
			}
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				auto it = m_idpath.find(task_id);
				if (it != m_idpath.end())
				{
					res.imgpath = std::move(it->second.first);
					wres = it->second.second;
					if (wres) *wres = res;
					m_idpath.erase(it);
					m_cv_idpath.notify_all();
				}
			}
			OnOCRResult(res);
		}
	}
	RemoveMMMojoReadInfo(request_info);
}

bool CWeChatOCR::wait_connection(int timeout)
{
	if (m_state == STATE_VALID)
		return true;
	if (m_state == STATE_INVALID)
		return false;

	if (!__super::wait_connection(timeout))
		return false;
	std::unique_lock<std::mutex> lock(m_mutex);
	if (m_state == STATE_INVALID)
		return false;
	if (m_state == STATE_PENDING)
	{
		bool ww = true;
		if (timeout < 0)
			m_cv_state.wait(lock, [this] {return m_state == STATE_VALID; });
		else
			ww = m_cv_state.wait_for(lock, std::chrono::milliseconds(timeout), [this] {return m_state == STATE_VALID; });
		if (!ww)
			return false;
	}
	return m_state == STATE_VALID;
}

bool CWeChatOCR::wait_done(int timeout)
{
	if (m_state == STATE_PENDING)
		wait_connection(timeout);
	if (m_state == STATE_INVALID)
		return true;
	if (m_state == STATE_PENDING)
		return false; // timeout.

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
