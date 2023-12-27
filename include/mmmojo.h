#pragma once

#ifndef MMMOJO_EXPORT
#define MMMOJO_EXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// method interface
typedef enum {
  kMMNone = 0,
  kMMPush,
  kMMPullReq,
  kMMPullResp,
  kMMShared,
} MMMojoInfoMethod;

// read interface
MMMOJO_EXPORT const void* GetMMMojoReadInfoRequest(const void* mmmojo_readinfo,
                                                   uint32_t* request_data_size);
MMMOJO_EXPORT const void* GetMMMojoReadInfoAttach(const void* mmmojo_readinfo,
                                                  uint32_t* attach_data_size);
MMMOJO_EXPORT int GetMMMojoReadInfoMethod(const void* mmmojo_readinfo);
MMMOJO_EXPORT bool GetMMMojoReadInfoSync(const void* mmmojo_readinfo);
MMMOJO_EXPORT void RemoveMMMojoReadInfo(const void* mmmojo_readinfo);

// write interface
//
// MMMojoInfoMethod::kMMPush & kMMPullReq & kMMPullResp
// using default max num bytes
// mojo/core/embedder/configuration.h
// Maximum data size of messages sent over message pipes, in bytes.
// size_t max_message_num_bytes = 256 * 1024 * 1024;
//
// MMMojoInfoMethod:kMMShared
// Maximum size of a single shared memory segment, in bytes.
// size_t max_shared_memory_num_bytes = 1024 * 1024 * 1024;
MMMOJO_EXPORT void* CreateMMMojoWriteInfo(int method, bool sync, uint32_t request_id);
MMMOJO_EXPORT void SetMMMojoWriteInfoMessagePipe(void* mmmojo_writeinfo, int num_of_message_pipe);
MMMOJO_EXPORT void SetMMMojoWriteInfoResponseSync(void* mmmojo_writeinfo, void** mmmojo_readinfo);
MMMOJO_EXPORT void* GetMMMojoWriteInfoRequest(void* mmmojo_writeinfo, size_t request_data_size);
MMMOJO_EXPORT void* GetMMMojoWriteInfoAttach(void* mmmojo_writeinfo, size_t attach_data_size);
MMMOJO_EXPORT bool SwapMMMojoWriteInfoCallback(void* mmmojo_writeinfo, void* mmmojo_readinfo);
MMMOJO_EXPORT bool SwapMMMojoWriteInfoMessage(void* mmmojo_writeinfo, void* mmmojo_readinfo);
MMMOJO_EXPORT bool SendMMMojoWriteInfo(void* mmmojo_env, void* mmmojo_writeinfo);
MMMOJO_EXPORT void RemoveMMMojoWriteInfo(void* mmmojo_writeinfo);

// env interface
typedef enum {
  kMMUserData = 0,
  kMMReadPush,
  kMMReadPull,
  kMMReadShared,
  kMMRemoteConnect,
  kMMRemoteDisconnect,
  kMMRemoteProcessLaunched,
  kMMRemoteProcessLaunchFailed,
  kMMRemoteMojoError,
} MMMojoEnvironmentCallbackType;

typedef enum {
  kMMHostProcess = 0,
  kMMLoopStartThread,
  kMMExePath,
  kMMLogPath,
  kMMLogToStderr,
  kMMAddNumMessagepipe,
  kMMSetDisconnectHandlers,
#if defined(WIN32)
  kMMDisableDefaultPolicy = 1000,
  kMMElevated,
  kMMCompatible,
#endif  // defined(WIN32)
} MMMojoEnvironmentInitParamType;

MMMOJO_EXPORT void* CreateMMMojoEnvironment();
MMMOJO_EXPORT void SetMMMojoEnvironmentCallbacks(void* mmmojo_env,
                                                 int type,
                                                 ...);
MMMOJO_EXPORT void SetMMMojoEnvironmentInitParams(void* mmmojo_env,
                                                  int type,
                                                  ...);
#if defined(WIN32)
MMMOJO_EXPORT void AppendMMSubProcessSwitchNative(void* mmmojo_env,
                                                  const char* switch_string,
                                                  const wchar_t* value);
#else
MMMOJO_EXPORT void AppendMMSubProcessSwitchNative(void* mmmojo_env,
                                                  const char* switch_string,
                                                  const char* value);
#endif  // defined(WIN32)
MMMOJO_EXPORT void StartMMMojoEnvironment(void* mmmojo_env);
MMMOJO_EXPORT void StopMMMojoEnvironment(void* mmmojo_env);
MMMOJO_EXPORT void RemoveMMMojoEnvironment(void* mmmojo_env);

// global interface
MMMOJO_EXPORT void InitializeMMMojo(int argc, const char* const* argv);
MMMOJO_EXPORT void ShutdownMMMojo();

#ifdef __cplusplus
}  // extern "C"
#endif
