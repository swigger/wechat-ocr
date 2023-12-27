#pragma once

namespace mmmojo
{
	//每一个组件的每一个请求都有一个自己的RequestId 组件会根据请求ID进行对应的操作

	//WeChatOCR组件
	enum RequestIdOCR
	{
		OCRPush = 1
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

class CMojoCall
{
protected:
	HMODULE m_mod = NULL;
	void* m_env = 0;
	// is the exe started and connected?
	bool m_connected = false;
	std::mutex m_mutex_conn; // mutex for conn.
	std::condition_variable m_cv_conn;

	/**
	 * @brief 对应Chromium源码中的base::CommandLine->AppendSwitchNative方法 用于添加一个'开关(Switch)'.
	 * 例如 "user-lib-dir" => X:\Foo 这样会在命令行参数添加一个"--user-lib-dir=X:\Foo"
	 */
	std::map<string, wstring> m_args;

protected:
	CMojoCall() = default;
	virtual ~CMojoCall();

	bool Init(LPCWSTR dir);
	bool Start(LPCWSTR exepath);
	void Stop();
	virtual bool wait_connection(int timeout);
	bool SendPbSerializedData(const void* pb_data, size_t data_size, int method, bool sync, uint32_t request_id);

	//call backs.
	virtual void ReadOnPush(uint32_t request_id, const void* request_info) {}
	virtual void ReadOnPull(uint32_t request_id, const void* request_info) {}
	virtual void ReadOnShared(uint32_t request_id, const void* request_info) {}
	virtual void OnRemoteConnect(bool is_connected);
	virtual void OnRemoteDisConnect();
	virtual void OnRemoteProcessLaunched() {}
	virtual void OnRemoteProcessLaunchFailed(int error_code) {}
	virtual void OnRemoteError(const void* errorbuf, int errorsize) {
		fprintf(stderr, "OnRemoteError: %s\n", (const char*)errorbuf);
	}
};
