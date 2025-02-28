#pragma once

// method interface
typedef enum {
  kMMNone = 0,
  kMMPush,
  kMMPullReq,
  kMMPullResp,
  kMMShared,
} MMMojoInfoMethod;

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
