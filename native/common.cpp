#include "steamwrap.h"

void hl_set_uid( vdynamic *out, int64 uid ) {
	union {
		vbyte b[8];
		int64 v;
	} data;
	data.v = uid;
	out->t = &hlt_bytes;
	out->v.ptr = hl_copy_bytes(data.b,8);
}

CSteamID hl_to_uid( vuid v ) {
	union {
		vbyte b[8];
		uint64 v;
	} data;
	memcpy(data.b,v,8);
	return CSteamID(data.v);
}

void dyn_call_result( vclosure *c, vdynamic *p, bool error ) {
	vdynamic b;
	vdynamic *args[2];
	args[0] = p;
	args[1] = &b;
	b.t = &hlt_bool;
	b.v.b = error;
	hl_dyn_call(c,args,2);
}

//just splits a string
void split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
}

//generates a parameter string array from comma-delimeted-values
SteamParamStringArray_t * getSteamParamStringArray(const char * str){
	std::string stdStr = str;

	//NOTE: this will probably fail if the string includes Unicode, but Steam tags probably don't support that?
	std::vector<std::string> v;
	split(stdStr, ',', v);

	SteamParamStringArray_t * params = new SteamParamStringArray_t;

	int count = v.size();

	params->m_nNumStrings = (int32) count;
	params->m_ppStrings = new const char *[count];

	for(int i = 0; i < count; i++) {
		params->m_ppStrings[i] = v[i].c_str();
	}

	return params;
}

void deleteSteamParamStringArray(SteamParamStringArray_t * params){
	for(int i = 0; i < params->m_nNumStrings; i++){
		delete params->m_ppStrings[i];
	}
	delete[] params->m_ppStrings;
	delete params;
}


//-----------------------------------------------------------------------------------------------------------
// Event
//-----------------------------------------------------------------------------------------------------------

vclosure *g_eventHandler = 0;

void SendEvent(event_type type, bool success, const char *data) {
	if (!g_eventHandler) return;
	if (g_eventHandler->hasValue)
		((void(*)(void*, event_type, bool, vbyte*))g_eventHandler->fun)(g_eventHandler->value, type, success, (vbyte*)data);
	else
		((void(*)(event_type, bool, vbyte*))g_eventHandler->fun)(type, success, (vbyte*)data);
}

void CallbackHandler::OnDownloadItem( DownloadItemResult_t *pCallback ){
	if (pCallback->m_unAppID != SteamUtils()->GetAppID()) return;

	std::ostringstream fileIDStream;
	PublishedFileId_t m_ugcFileID = pCallback->m_nPublishedFileId;
	fileIDStream << m_ugcFileID;
	SendEvent(ItemDownloaded, pCallback->m_eResult == k_EResultOK, fileIDStream.str().c_str());
}

void CallbackHandler::OnItemInstalled( ItemInstalled_t *pCallback ){
	if (pCallback->m_unAppID != SteamUtils()->GetAppID()) return;

	std::ostringstream fileIDStream;
	PublishedFileId_t m_ugcFileID = pCallback->m_nPublishedFileId;
	fileIDStream << m_ugcFileID;
	SendEvent(ItemDownloaded, true, fileIDStream.str().c_str());
}

CallbackHandler* s_callbackHandler = NULL;

bool CheckInit(){
	return SteamUser() && SteamUser()->BLoggedOn() && SteamUserStats() && (s_callbackHandler != 0) && (g_eventHandler != 0);
}

extern "C"
{

HL_PRIM bool HL_NAME(init)(vclosure *onEvent){
	bool result = SteamAPI_Init();
	if (result)	{
		g_eventHandler = onEvent;
		// TODO gc_root
		s_callbackHandler = new CallbackHandler();
	}
	return result;
}
DEFINE_PRIM(_BOOL, init, _FUN(_VOID, _I32 _BOOL _BYTES));

HL_PRIM void HL_NAME(set_notification_position)( ENotificationPosition pos ) {
	SteamUtils()->SetOverlayNotificationPosition(pos);
}

DEFINE_PRIM(_VOID, set_notification_position, _I32);

HL_PRIM void HL_NAME(shutdown)(){
	SteamAPI_Shutdown();
	// TODO gc_root
	g_eventHandler = NULL;
	delete s_callbackHandler;
	s_callbackHandler = NULL;
}
DEFINE_PRIM(_VOID, shutdown, _NO_ARG);

HL_PRIM void HL_NAME(run_callbacks)(){
	SteamAPI_RunCallbacks();
}
DEFINE_PRIM(_VOID, run_callbacks, _NO_ARG);

HL_PRIM bool HL_NAME(open_overlay)(vbyte *url){
	if (!CheckInit()) return false;

	SteamFriends()->ActivateGameOverlayToWebPage((char*)url);
	return true;
}
DEFINE_PRIM(_BOOL, open_overlay, _BYTES);

//-----------------------------------------------------------------------------------------------------------

HL_PRIM vbyte *HL_NAME(get_persona_name)(){
	if(!CheckInit()) return (vbyte*)"Unknown";
	return (vbyte*)SteamFriends()->GetPersonaName();
}
DEFINE_PRIM(_BYTES, get_persona_name, _NO_ARG);

HL_PRIM vbyte *HL_NAME(get_steam_id)(){
	if(!CheckInit()) return (vbyte*)"0";

	CSteamID userId = SteamUser()->GetSteamID();

	std::ostringstream returnData;
	returnData << userId.ConvertToUint64();

	return (vbyte*)returnData.str().c_str();
}
DEFINE_PRIM(_BYTES, get_steam_id, _NO_ARG);

HL_PRIM bool HL_NAME(restart_app_if_necessary)(int appId){
	return SteamAPI_RestartAppIfNecessary(appId);
}
DEFINE_PRIM(_BOOL, restart_app_if_necessary, _I32);

HL_PRIM bool HL_NAME(is_overlay_enabled)(){
	return SteamUtils()->IsOverlayEnabled();
}
DEFINE_PRIM(_BOOL, is_overlay_enabled, _NO_ARG);

HL_PRIM bool HL_NAME(boverlay_needs_present)(){
	return SteamUtils()->BOverlayNeedsPresent();
}
DEFINE_PRIM(_BOOL, boverlay_needs_present, _NO_ARG);

HL_PRIM bool HL_NAME(is_steam_in_big_picture_mode)(){
	return SteamUtils()->IsSteamInBigPictureMode();
}
DEFINE_PRIM(_BOOL, is_steam_in_big_picture_mode, _NO_ARG);

HL_PRIM bool HL_NAME(is_steam_running)(){
	return SteamAPI_IsSteamRunning();
}
DEFINE_PRIM(_BOOL, is_steam_running, _NO_ARG);

HL_PRIM vbyte *HL_NAME(get_current_game_language)(){
	return (vbyte*)SteamApps()->GetCurrentGameLanguage();
}
DEFINE_PRIM(_BYTES, get_current_game_language, _NO_ARG);

HL_PRIM void HL_NAME(cancel_call_result)( CClosureCallResult<int> *m_call ) {
	m_call->Cancel();
	delete m_call;
}

DEFINE_PRIM(_VOID, cancel_call_result, _CRESULT);

} // extern "C"

