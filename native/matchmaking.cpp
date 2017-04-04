#include "steamwrap.h"

static void on_lobby_list( vclosure *c, LobbyMatchList_t *result, bool error ) {
	vdynamic d;
	d.t = &hlt_i32;
	d.v.i = error ? -1 : result->m_nLobbiesMatching;
	dyn_call_result(c,&d,error);
}

HL_PRIM CClosureCallResult<LobbyMatchList_t>* HL_NAME(request_lobby_list)( vclosure *closure ) {
	if( !CheckInit() ) return NULL;
	ASYNC_CALL(SteamMatchmaking()->RequestLobbyList(), LobbyMatchList_t, on_lobby_list);
	return m_call;
}
DEFINE_PRIM(_CRESULT, request_lobby_list, _CALLB(_I32));

static void on_lobby_created( vclosure *c, LobbyCreated_t *result, bool error ) {
	vdynamic d;
	hl_set_uid(&d,error ? 0 : result->m_ulSteamIDLobby);
	dyn_call_result(c,&d,error);
}

HL_PRIM CClosureCallResult<LobbyCreated_t>* HL_NAME(create_lobby)( ELobbyType lobbyType, int maxMembers, vclosure *closure ) {
	ASYNC_CALL(SteamMatchmaking()->CreateLobby(lobbyType,maxMembers), LobbyCreated_t, on_lobby_created);
	return m_call;
}
DEFINE_PRIM(_CRESULT, create_lobby, _I32 _I32 _CALLB(_UID));

HL_PRIM bool HL_NAME(request_lobby_data)( vuid uid ) {
	return SteamMatchmaking()->RequestLobbyData(hl_to_uid(uid));
}
DEFINE_PRIM(_BOOL, request_lobby_data, _UID);

