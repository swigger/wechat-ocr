#pragma once
#include "mojocall.h"

class CWeChatOCR : protected CMojoCall
{
public:
	struct text_block_t {
		float left, top, right, bottom;
		float rate;
		string text;
	};
	struct result_t {
		string imgpath;
		int errcode;
		int width, height;
		std::vector<text_block_t> ocr_response;
	};
protected:
	std::wstring m_exe, m_wcdir;
	std::mutex m_mutex;
	std::condition_variable m_cv_state, m_cv_idpath;
	enum state_t { STATE_INVALID, STATE_PENDING, STATE_VALID } m_state = STATE_INVALID;
	std::map<uint64_t, std::pair<string, result_t*>> m_idpath;

public:
	CWeChatOCR(LPCWSTR exe, LPCWSTR wcdir);
	~CWeChatOCR() = default;

	int state() const { return m_state; }
	bool wait_connection(int timeout) override;
	bool wait_done(int timeout); //wait for all pending ocr done.
	bool doOCR(crefstr imgpath, result_t* res = nullptr);

protected:
	virtual void OnOCRResult(result_t& ocr_response);
	void ReadOnPush(uint32_t request_id, const void* request_info) override;
};
