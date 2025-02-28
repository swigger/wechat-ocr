#pragma once
#include "mmmojo.h"

#ifdef _WIN32
typedef std::wstring tstring;
#else
typedef const char* LPCTSTR;
typedef void* HMODULE;
typedef std::string tstring;
#endif

#ifndef EXPORTED_API
# ifdef _WIN32
#  define EXPORTED_API __declspec(dllexport)
#  pragma warning(disable : 4251)
# else
#  define EXPORTED_API
# endif
#endif

#ifndef WIN_POSIX
# ifdef _WIN32
#  define WIN_POSIX(a,b) a
# else
#  define WIN_POSIX(a,b) b
# endif
#endif

namespace mmmojo
{
	//每一个组件的每一个请求都有一个自己的RequestId 组件会根据请求ID进行对应的操作

	//WeChatOCR组件
	enum RequestIdOCR3
	{
		OCRPush = 1,
	};
	enum RequestIdOCR4
	{
		HAND_SHAKE = 10001,
		REQ_OCR = 10010,
		RESP_OCR = 10011,
		PUT_UserInfoMessage = 20012,
	};

	//WeChatUtility组件
	enum RequestIdUtility
	{
		UtilityHiPush = 10001,				//是Utility启动发送的
		UtilityInitPullReq = 10002,			//初始化请求
		UtilityInitPullResp = 10003,		//回复创建的都是Shared类型的info, 但是调用了SwapMMMojoWriteInfoCallback, 所以回调的还是Pull
		UtilityResampleImagePullReq = 10010,
		UtilityResampleImagePullResp = 10011,
		UtilityDecodeImagePullReq = 10020,
		UtilityDecodeImagePullResp = 10021,
		UtilityPicQRScanPullReq = 10030,	//10030是点击OCR时(也是打开图片时)发送的请求, 参数是图片路径
		UtilityQRScanPullReq = 10031,		//10031是截图框选时发送的请求, 参数应该是某种编码后的图片数据
		UtilityQRScanPullResp = 10032,		//这两种请求的返回ID都是10032
		UtilityTextScanPushResp = 10040		//TextScan具体在扫什么不是很清楚 可能是用来判断图片上是否有文字
	};

	//ThumbPlayer组件
	enum RequestIdPlayer
	{
		PlayerHiPush = 10001,								//ThumbPlayer启动时发送的
		PlayerInitPullReq = 10002,							//PlayerMgr::Init
		PlayerInitPullResp = 10003,							//PlayerMgr::Init
		PlayerInitPlayerCorePush = 10010,
		PlayerCreatePlayerCorePullReq = 10011,				//PlayerMgr::CreatePlayerCore
		PlayerCreatePlayerCorePullResp = 10012,				//PlayerMgr::CreatePlayerCore
		PlayerDestroyPlayerCorePush = 10013,				//PlayerMgr::DestroyCore
		PlayerPrepareAsyncPush = 10014,						//PlayerMgr::PrepareCore
		PlayerStartPush = 10015,							//PlayerMgr::StartCore
		PlayerStopPush = 10016,								//PlayerMgr::StopCore
		PlayerPausePush = 10017,							//PlayerMgr::PauseCore
		PlayerResumePush = 10018,							//PlayerMgr::ResumeCore
		PlayerSetAudioMutePush = 10019,					//PlayerMgr::AudioMuteCore
		PlayerSeekToAsyncPush = 10020,						//PlayerMgr::SeekToCore
		PlayerGetCurrentPositionMsPullReq = 10021,			//PlayerMgr::GetCurrentPositionMsCore
		PlayerGetCurrentPositionMsPullResp = 10022,			//PlayerMgr::GetCurrentPositionMsCore
		PlayerSetVideoSurfacePush = 10023,					//PlayerMgr::VideoSurfaceCore
		PlayerSetAudioVolumePush = 10024,					//PlayerMgr::AudioVolumeCore
		PlayerSetDataSourcePush = 10025,					//PlayerMgr::ReadyDataSourceCore
		PlayerSetLoaderSourcePush = 10026,					//PlayerMgr::DownloadDataSourceCore
		PlayerSetRepeatPush = 10027,						//PlayerMgr::RepeatCore
		PlayerResizePush = 10028,							//PlayerMgr::ResizeCore
		PlayerSetPlaySpeedRatio = 10029,					//PlayerMgr::SpeedRatioCore
		PlayerInfoPush = 10030,
		PlayerErrorPlayerPush = 10031,
		PlayerVideoSizeChangedPush = 10032,
		PlayerStopAsyncCompletedPush = 10033,
		PlayerStateChangePush = 10034,
		PlayerSeekCompletedPush = 10035,
		PlayerCompletedPush = 10036,
		PlayerStartTaskProxyPush = 10050,
		PlayerStartRequestProxyPush = 10051,
		PlayerCloseRequestProxyPush = 10052,
		PlayerPollingDatProxyPullReq = 10053,
		PlayerPollingDatProxyPullResp = 10054
	};
}

class EXPORTED_API CMojoCall
{
public:
	static constexpr int MJC_FAILED = -1;
	static constexpr int MJC_PENDING = 0;
	static constexpr int MJC_CONNECTED = 1;

protected:
	HMODULE m_mod = NULL;
	bool m_should_free_mod = false;
	void* m_env = 0;
	int m_state = MJC_PENDING;
	std::mutex m_mutex_state; // mutex for conn.
	std::condition_variable m_cv_state;

	/**
	 * @brief 对应Chromium源码中的base::CommandLine->AppendSwitchNative方法 用于添加一个'开关(Switch)'.
	 * 例如 "user-lib-dir" => X:\Foo 这样会在命令行参数添加一个"--user-lib-dir=X:\Foo"
	 */
	std::map<string, tstring> m_args;

protected:
	CMojoCall();
	virtual ~CMojoCall();

	bool Init(LPCTSTR dir);
	bool Start(LPCTSTR exepath);
	void Stop();
	virtual bool wait_connection(int timeout);
	bool SendPbSerializedData(const void* pb_data, size_t data_size, int method, bool sync, uint32_t request_id);

	//call backs.
	virtual void ReadOnPush(uint32_t request_id, std::span<std::byte> request_info) {}
	virtual void ReadOnPull(uint32_t request_id, std::span<std::byte> request_info) {}
	virtual void ReadOnShared(uint32_t request_id, std::span<std::byte> request_info) {}
	virtual void OnRemoteConnect(bool is_connected);
	virtual void OnRemoteDisConnect();
	virtual void OnRemoteProcessLaunched() {}
	virtual void OnRemoteProcessLaunchFailed(int error_code);
	virtual void OnRemoteError(const void* errorbuf, int errorsize) {
		fprintf(stderr, "OnRemoteError: %.*s\n", errorsize, (const char*)errorbuf);
	}
};

namespace util
{
	template <class T, class FT>
	requires std::is_pointer_v<FT>&& std::is_function_v<typename std::remove_pointer<FT>::type> && (std::is_pointer_v<T> || std::is_integral_v<T>)
	struct auto_del_t {
		T object;
		FT caller;

		auto_del_t(T obj, FT call) : object(obj), caller(call) {}
		~auto_del_t() { if (caller) caller(object); }
		auto_del_t(const auto_del_t&) = delete;
		auto_del_t& operator=(const auto_del_t&) = delete;
		auto_del_t(auto_del_t &&) noexcept = delete;
		auto_del_t& operator=(auto_del_t&&) noexcept = delete;
	};

	inline tstring to_tstr(const char* src, bool isutf8=true)
	{
#ifdef _WIN32
		int cp = isutf8 ? CP_UTF8 : CP_ACP;
		int len = MultiByteToWideChar(cp, 0, src, -1, NULL, 0);
		wstring ret;
		if (len > 0) {
			ret.resize(len);
			MultiByteToWideChar(cp, 0, src, -1, &ret[0], len);
		}
		return ret;
#else
		return src;
#endif
	}
}
