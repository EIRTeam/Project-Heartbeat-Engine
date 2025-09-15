#ifdef USE_STEAMWORKS_STUB

#include "steam/steam_api_flat.h"

S_API void S_CALLTYPE SteamAPI_ManualDispatch_Init() {}

S_API void S_CALLTYPE SteamAPI_ManualDispatch_RunFrame(HSteamPipe hSteamPipe) {}

S_API bool S_CALLTYPE SteamAPI_ManualDispatch_GetNextCallback(HSteamPipe hSteamPipe, CallbackMsg_t *pCallbackMsg) {
	return false;
}

/// You must call this after dispatching the callback, if SteamAPI_ManualDispatch_GetNextCallback returns true.
S_API void S_CALLTYPE SteamAPI_ManualDispatch_FreeLastCallback(HSteamPipe hSteamPipe) {}

S_API bool S_CALLTYPE SteamAPI_ManualDispatch_GetAPICallResult(HSteamPipe hSteamPipe, SteamAPICall_t hSteamAPICall, void *pCallback, int cubCallback, int iCallbackExpected, bool *pbFailed) {
	return false;
}

S_API bool S_CALLTYPE SteamAPI_Init() {
	return false;
}

S_API void S_CALLTYPE SteamAPI_Shutdown() {}

S_API HSteamPipe S_CALLTYPE SteamAPI_GetHSteamPipe() {
	return 0;
}

S_API void *S_CALLTYPE SteamInternal_ContextInit(void *pContextInitData) {
	return nullptr;
}

S_API void *S_CALLTYPE SteamInternal_CreateInterface(const char *ver) {
	return nullptr;
}

#endif