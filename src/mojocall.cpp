#include "stdafx.h"
#include <stdio.h>
#include "mmmojo_funcs.h"
#include "mojocall.h"
#ifndef _WIN32
# include <dlfcn.h>
#endif

#ifdef _DEBUG
# define DBG_PRINT(fmt, ...) fprintf(stderr, "[DEBUG]" fmt, __VA_ARGS__)
#else
# define DBG_PRINT(fmt, ...)
#endif

namespace midlevel
{
#define LAZY_FUNCTION(x) decltype(&::x) x = nullptr
	LAZY_FUNCTION(CreateMMMojoEnvironment);
	LAZY_FUNCTION(AppendMMSubProcessSwitchNative);
	LAZY_FUNCTION(CreateMMMojoWriteInfo);
	LAZY_FUNCTION(GetMMMojoReadInfoRequest);
	LAZY_FUNCTION(GetMMMojoWriteInfoRequest);
	LAZY_FUNCTION(RemoveMMMojoEnvironment);
	LAZY_FUNCTION(RemoveMMMojoReadInfo);
	LAZY_FUNCTION(RemoveMMMojoWriteInfo);
	LAZY_FUNCTION(SendMMMojoWriteInfo);
	LAZY_FUNCTION(SetMMMojoEnvironmentCallbacks);
	LAZY_FUNCTION(SetMMMojoEnvironmentInitParams);
	LAZY_FUNCTION(StartMMMojoEnvironment);
	LAZY_FUNCTION(StopMMMojoEnvironment);
	LAZY_FUNCTION(InitializeMMMojo);

	template<class T>
	bool init_func(HMODULE mod, T*& func, const char* name)
	{
#ifdef _WIN32
		func = (T*)GetProcAddress(mod, name);
#else
		func = (T*)dlsym(mod, name);
#endif
		return !!func;
	}

	static int init_funcs(HMODULE mod)
	{
		int failcnt = 0;
#define INIT_LAZY_FUNCTION(x) failcnt += init_func(mod, x, #x) ? 0 : 1;
		INIT_LAZY_FUNCTION(CreateMMMojoEnvironment);
		INIT_LAZY_FUNCTION(AppendMMSubProcessSwitchNative);
		INIT_LAZY_FUNCTION(CreateMMMojoWriteInfo);
		INIT_LAZY_FUNCTION(GetMMMojoReadInfoRequest);
		INIT_LAZY_FUNCTION(GetMMMojoWriteInfoRequest);
		INIT_LAZY_FUNCTION(RemoveMMMojoEnvironment);
		INIT_LAZY_FUNCTION(RemoveMMMojoReadInfo);
		INIT_LAZY_FUNCTION(RemoveMMMojoWriteInfo);
		INIT_LAZY_FUNCTION(SendMMMojoWriteInfo);
		INIT_LAZY_FUNCTION(SetMMMojoEnvironmentCallbacks);
		INIT_LAZY_FUNCTION(SetMMMojoEnvironmentInitParams);
		INIT_LAZY_FUNCTION(StartMMMojoEnvironment);
		INIT_LAZY_FUNCTION(StopMMMojoEnvironment);
		INIT_LAZY_FUNCTION(InitializeMMMojo);
		return failcnt;
	}

	class CMojoCall_Mid : public CMojoCall
	{
	public:
		bool Init(LPCTSTR wcdir)
		{
#ifdef _WIN32
			if (!m_mod) {
				// find mmmojo(_64).dll
				std::wstring mojo_dll = wcdir;
				if (mojo_dll.empty()) return false;
				if (mojo_dll.back() != L'\\') mojo_dll += L'\\';
				size_t osz = mojo_dll.size();
# ifdef _WIN64
				mojo_dll += L"mmmojo_64.dll";
# else
				mojo_dll += L"mmmojo.dll";
# endif
				auto mod = GetModuleHandleW(mojo_dll.c_str() + osz);
				if (!mod) {
					mod = LoadLibraryW(mojo_dll.c_str());
					if (mod == NULL) return false;
					m_should_free_mod = true;
				}
				if (init_funcs(mod)) {
					if (m_should_free_mod) FreeLibrary(mod);
					m_should_free_mod = false;
					return false;
				}
				m_mod = mod;
			}
#else
			string folder = wcdir;
			if (!folder.empty() && folder.back() != '/') folder += '/';
			folder += "libmmmojo.so";
			void* mod = dlopen(folder.c_str(), RTLD_LAZY);
			if (!mod) {
				fprintf(stderr, "dlopen failed: %s\n", dlerror());
				return false;
			}
			if (init_funcs(mod)) {
				dlclose(mod);
				return false;
			}
			m_should_free_mod = true;
			m_mod = mod;
#endif
			InitializeMMMojo(0, 0);
			return true;
		}

		bool Start(LPCTSTR exepath)
		{
			if (!m_env) {
				// 创建MMMojo环境
				auto env = CreateMMMojoEnvironment();
				if (!env) return false;

#define self ((CMojoCall_Mid*)user_data)
				// 设置回调函数
				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMUserData, this);
				auto ReadOnPush = +[](uint32_t request_id, const void* request_info, void* user_data) {
					DBG_PRINT("ReadOnPush: %u\n", request_id);
					uint32_t pb_size = 0;
					const void* pb_data = GetMMMojoReadInfoRequest(request_info, &pb_size);
					self->ReadOnPush(request_id, std::span((std::byte*)pb_data, pb_size));
					RemoveMMMojoReadInfo(request_info);
				};
				auto ReadOnPull = +[](uint32_t request_id, const void* request_info, void* user_data) {
					DBG_PRINT("ReadOnPull: %u\n", request_id);
					uint32_t pb_size = 0;
					const void* pb_data = GetMMMojoReadInfoRequest(request_info, &pb_size);
					self->ReadOnPull(request_id, std::span((std::byte*)pb_data, pb_size));
					RemoveMMMojoReadInfo(request_info);
				};
				auto ReadOnShared = +[](uint32_t request_id, const void* request_info, void* user_data) {
					DBG_PRINT("ReadOnShared: %u\n", request_id);
					uint32_t pb_size = 0;
					const void* pb_data = GetMMMojoReadInfoRequest(request_info, &pb_size);
					return self->ReadOnShared(request_id, std::span((std::byte*)pb_data, pb_size));
					RemoveMMMojoReadInfo(request_info);
				};
				auto OnConnect = +[](bool is_connected, void* user_data) {
					DBG_PRINT("OnConnect: %d\n", is_connected);
					return self->OnRemoteConnect(is_connected);
				};
				auto OnDisConnect = +[](void* user_data) {
					DBG_PRINT("OnDisConnect\n");
					return self->OnRemoteDisConnect();
				};
				auto OnProcessLaunched = +[](void* user_data) {
					DBG_PRINT("OnProcessLaunched\n");
					return self->OnRemoteProcessLaunched();
				};
				auto OnProcessLaunchFailed = +[](int error_code, void* user_data) {
					DBG_PRINT("OnProcessLaunchFailed: %d\n", error_code);
					return self->OnRemoteProcessLaunchFailed(error_code);
				};
				auto OnError = +[](const void* errorbuf, int errorsize, void* user_data) {
					DBG_PRINT("OnError: %.*s\n", errorsize, (const char*)errorbuf);
					return self->OnRemoteError(errorbuf, errorsize);
				};

				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMReadPush, ReadOnPush);
				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMReadPull, ReadOnPull);
				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMReadShared, ReadOnShared);
				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMRemoteConnect, OnConnect);
				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMRemoteDisconnect, OnDisConnect);
				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMRemoteProcessLaunched, OnProcessLaunched);
				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMRemoteProcessLaunchFailed, OnProcessLaunchFailed);
				SetMMMojoEnvironmentCallbacks(env, MMMojoEnvironmentCallbackType::kMMRemoteMojoError, OnError);
				m_env = env;
			}

			// 设置启动所需参数
			SetMMMojoEnvironmentInitParams(m_env, MMMojoEnvironmentInitParamType::kMMHostProcess, true);
			SetMMMojoEnvironmentInitParams(m_env, MMMojoEnvironmentInitParamType::kMMExePath, exepath);

			// 设置SwitchNative命令行参数
			for (const auto& [k, v] : m_args) {
				AppendMMSubProcessSwitchNative(m_env, k.c_str(), v.c_str());
			}
			StartMMMojoEnvironment(m_env);
			return true;
		}

		void Stop()
		{
			if (m_env) {
				StopMMMojoEnvironment(m_env);
				RemoveMMMojoEnvironment(m_env);
				m_env = 0;
			}
		}

		bool SendPbSerializedData(const void* pb_data, size_t data_size, int method, bool sync, uint32_t request_id)
		{
			if (data_size < 0) return false;

			void* write_info = CreateMMMojoWriteInfo(method, sync, request_id);
			if (data_size != 0) {
				void* request = GetMMMojoWriteInfoRequest(write_info, data_size);
				// TODO: 这里是否有内存泄漏? request，write_info指针会释放吗?
				memcpy(request, pb_data, data_size);  // 写入protobuf数据
				return SendMMMojoWriteInfo(m_env, write_info);
			}
			RemoveMMMojoWriteInfo(write_info);
			return false;
		}
	};
}  // namespace midlevel

CMojoCall::CMojoCall()
{
}

CMojoCall::~CMojoCall()
{
	Stop();
	if (m_mod && m_should_free_mod) {
		/* I really want to release the module here, unfortunately stopping mmmojo env does not stop the threads it created,
		so free the module here will inevitably cause a crash. So I can only ignore it.
		If you want to load different mmmojo modules, you may have to find another way. . .
		*/
#ifdef _WIN32
		// FreeLibrary(m_mod);
#else
		// dlclose(m_mod);
#endif
	}
	m_mod = 0;
	m_should_free_mod = false;
}

bool CMojoCall::Init(LPCTSTR wcdir)
{
	return ((midlevel::CMojoCall_Mid*)this)->Init(wcdir);
}

bool CMojoCall::Start(LPCTSTR exepath)
{
	return ((midlevel::CMojoCall_Mid*)this)->Start(exepath);
}

void CMojoCall::Stop()
{
	((midlevel::CMojoCall_Mid*)this)->Stop();
}

bool CMojoCall::SendPbSerializedData(const void* pb_data, size_t data_size, int method, bool sync, uint32_t request_id)
{
	return ((midlevel::CMojoCall_Mid*)this)->SendPbSerializedData(pb_data, data_size, method, sync, request_id);
}

void CMojoCall::OnRemoteConnect(bool is_connected)
{
	std::lock_guard<std::mutex> lock(m_mutex_state);
	m_state = is_connected ? MJC_CONNECTED : MJC_FAILED;
	m_cv_state.notify_all();
}

void CMojoCall::OnRemoteDisConnect()
{
	std::lock_guard<std::mutex> lock(m_mutex_state);
	m_state = MJC_FAILED;
	m_cv_state.notify_all();
}

bool CMojoCall::wait_connection(int timeout)
{
	if (timeout < 0) {
		std::unique_lock<std::mutex> lock(m_mutex_state);
		m_cv_state.wait(lock, [this] { return m_state != MJC_PENDING; });
	} else {
		std::unique_lock<std::mutex> lock(m_mutex_state);
		if (!m_cv_state.wait_for(lock, std::chrono::milliseconds(timeout), [this] { return m_state != MJC_PENDING; })) {
			return false;
		}
	}
	return m_state >= MJC_CONNECTED;
}

void CMojoCall::OnRemoteProcessLaunchFailed(int error_code)
{
	std::lock_guard<std::mutex> lock(m_mutex_state);
	if (m_state == MJC_PENDING) {
		m_state = MJC_FAILED;
		m_cv_state.notify_all();
	}
}
