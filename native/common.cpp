#include "steamwrap.h"

void hl_set_uid( vdynamic *out, int64 uid ) {
	out->t = &hlt_uid;
	out->v.ptr = hl_of_uid(CSteamID((uint64)uid));
}

CSteamID hl_to_uid( vuid v ) {
	union {
		vbyte b[8];
		uint64 v;
	} data;
	memcpy(data.b,v,8);
	return CSteamID(data.v);
}

vuid hl_of_uid( CSteamID uid ) {
	union {
		vbyte b[8];
		uint64 v;
	} data;
	data.v = uid.ConvertToUint64();
	return (vuid)hl_copy_bytes(data.b, 8);
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


CallbackHandler* s_callbackHandler = NULL;
static vclosure *s_globalEvent = NULL;

bool CheckInit(){
	return SteamUser() && SteamUser()->BLoggedOn() && SteamUserStats() && (s_callbackHandler != 0) && (g_eventHandler != 0);
}

void GlobalEvent( int id, vdynamic *v ) {
	vdynamic i;
	vdynamic *args[2];
	i.t = &hlt_i32;
	i.v.i = id;
	args[0] = &i;
	args[1] = v;
	hl_dyn_call(s_globalEvent, args, 2);
}

#define EVENT_DECL(name,type) void CallbackHandler::On##name( type *t ) { GlobalEvent(type::k_iCallback, Encode##name(t)); }
#define GLOBAL_EVENTS
#include "events.h"
#undef GLOBAL_EVENTS

vdynamic *CallbackHandler::EncodeOverlayActivated(GameOverlayActivated_t *d) {
	HLValue ret;
	ret.Set("active", d->m_bActive);
	return ret.value;
}

HL_PRIM bool HL_NAME(init)( vclosure *onEvent, vclosure *onGlobalEvent ){
	bool result = SteamAPI_Init();
	if (result)	{
		g_eventHandler = onEvent;
		s_callbackHandler = new CallbackHandler();
		s_globalEvent = onGlobalEvent;
		hl_add_root(&s_globalEvent);
	}
	return result;
}

HL_PRIM void HL_NAME(set_notification_position)( ENotificationPosition pos ) {
	if( !CheckInit() ) return;
	SteamUtils()->SetOverlayNotificationPosition(pos);
}

HL_PRIM void HL_NAME(shutdown)(){
	SteamAPI_Shutdown();
	// TODO gc_root
	g_eventHandler = NULL;
	delete s_callbackHandler;
	s_callbackHandler = NULL;
}

HL_PRIM void HL_NAME(run_callbacks)(){
	SteamAPI_RunCallbacks();
}

HL_PRIM bool HL_NAME(open_overlay)(vbyte *url){
	if (!CheckInit()) return false;

	SteamFriends()->ActivateGameOverlayToWebPage((char*)url);
	return true;
}

DEFINE_PRIM(_BOOL, init, _FUN(_VOID, _I32 _BOOL _BYTES) _FUN(_VOID, _I32 _DYN));
DEFINE_PRIM(_VOID, set_notification_position, _I32);
DEFINE_PRIM(_VOID, shutdown, _NO_ARG);
DEFINE_PRIM(_VOID, run_callbacks, _NO_ARG);
DEFINE_PRIM(_BOOL, open_overlay, _BYTES);

//-----------------------------------------------------------------------------------------------------------

HL_PRIM vuid HL_NAME(get_steam_id)(){
	return hl_of_uid(SteamUser()->GetSteamID());
}

HL_PRIM bool HL_NAME(restart_app_if_necessary)(int appId){
	return SteamAPI_RestartAppIfNecessary(appId);
}

HL_PRIM bool HL_NAME(is_overlay_enabled)(){
	return SteamUtils()->IsOverlayEnabled();
}

HL_PRIM bool HL_NAME(boverlay_needs_present)(){
	return SteamUtils()->BOverlayNeedsPresent();
}

HL_PRIM bool HL_NAME(is_steam_in_big_picture_mode)(){
	return SteamUtils()->IsSteamInBigPictureMode();
}

HL_PRIM bool HL_NAME(is_steam_running)(){
	return SteamAPI_IsSteamRunning();
}

HL_PRIM vbyte *HL_NAME(get_current_game_language)(){
	return (vbyte*)SteamApps()->GetCurrentGameLanguage();
}

HL_PRIM void HL_NAME(cancel_call_result)( CClosureCallResult<int> *m_call ) {
	m_call->Cancel();
	delete m_call;
}

DEFINE_PRIM(_UID, get_steam_id, _NO_ARG);
DEFINE_PRIM(_BOOL, restart_app_if_necessary, _I32);
DEFINE_PRIM(_BOOL, is_overlay_enabled, _NO_ARG);
DEFINE_PRIM(_BOOL, boverlay_needs_present, _NO_ARG);
DEFINE_PRIM(_BOOL, is_steam_in_big_picture_mode, _NO_ARG);
DEFINE_PRIM(_BOOL, is_steam_running, _NO_ARG);
DEFINE_PRIM(_BYTES, get_current_game_language, _NO_ARG);
DEFINE_PRIM(_VOID, cancel_call_result, _CRESULT);
