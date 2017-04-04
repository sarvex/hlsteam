#include "steamwrap.h"

class Callbacks {
public:
	Callbacks() : LobbyData(this,&Callbacks::OnLobbyData) {}
	EVENT_CALLBACK( LobbyData, LobbyDataUpdate_t );
	vdynamic *EncodeLobbyData( LobbyDataUpdate_t *d ) {
		if( !d->m_bSuccess ) return NULL;
		HLValue ret;
		ret.Set("lobby",d->m_ulSteamIDLobby);
		if( d->m_ulSteamIDMember != d->m_ulSteamIDLobby ) ret.Set("member",d->m_ulSteamIDMember);
		return ret.value;
	}
};

static Callbacks *match_callbacks = NULL;

HL_PRIM void HL_NAME(match_init)() {
	match_callbacks = new Callbacks();
}

DEFINE_PRIM(_VOID, match_init, _NO_ARG);

// --------- Lobby Search / Create --------------------------

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

static void on_lobby_created( vclosure *c, LobbyCreated_t *result, bool error ) {
	vdynamic d;
	hl_set_uid(&d,error ? 0 : result->m_ulSteamIDLobby);
	dyn_call_result(c,&d,error);
}

HL_PRIM CClosureCallResult<LobbyCreated_t>* HL_NAME(create_lobby)( ELobbyType lobbyType, int maxMembers, vclosure *closure ) {
	ASYNC_CALL(SteamMatchmaking()->CreateLobby(lobbyType,maxMembers), LobbyCreated_t, on_lobby_created);
	return m_call;
}

DEFINE_PRIM(_CRESULT, request_lobby_list, _CALLB(_I32));
DEFINE_PRIM(_CRESULT, create_lobby, _I32 _I32 _CALLB(_UID));


// --------- Lobby Data --------------------------

HL_PRIM bool HL_NAME(request_lobby_data)( vuid uid ) {
	return SteamMatchmaking()->RequestLobbyData(hl_to_uid(uid));
}

HL_PRIM const char *HL_NAME(get_lobby_data)( vuid uid, const char *key ) {
	return SteamMatchmaking()->GetLobbyData(hl_to_uid(uid),key);
}

HL_PRIM void HL_NAME(set_lobby_data)( vuid uid, const char *key, const char *value ) {
	SteamMatchmaking()->SetLobbyData(hl_to_uid(uid), key, value);
}

HL_PRIM void HL_NAME(delete_lobby_data)( vuid uid, const char *key ) {
	SteamMatchmaking()->DeleteLobbyData(hl_to_uid(uid),key);
}

HL_PRIM int HL_NAME(get_lobby_data_count)( vuid uid ) {
	return SteamMatchmaking()->GetLobbyDataCount(hl_to_uid(uid));
}

HL_PRIM bool HL_NAME(get_lobby_data_byindex)( vuid uid, int index, char *key, int ksize, char *value, int vsize ) {
	return SteamMatchmaking()->GetLobbyDataByIndex(hl_to_uid(uid), index, key, ksize, value, vsize);
}

DEFINE_PRIM(_BOOL, request_lobby_data, _UID);
DEFINE_PRIM(_BYTES, get_lobby_data, _UID _BYTES);
DEFINE_PRIM(_VOID, set_lobby_data, _UID _BYTES _BYTES);
DEFINE_PRIM(_VOID, delete_lobby_data, _UID _BYTES);
DEFINE_PRIM(_I32, get_lobby_data_count, _UID);
DEFINE_PRIM(_BOOL, get_lobby_data_byindex, _UID _I32 _BYTES _I32 _BYTES _I32);
