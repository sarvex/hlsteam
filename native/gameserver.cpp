#include "steamwrap.h"

class GameServerHandler {
	std::map<std::string, void*> m;
public:
	GameServerHandler() :
#	define EVENT_DECL(name,type) m_##name(this,&GameServerHandler::On##name),
#	include "serverevents.h"
		m()
	{}

#	define EVENT_DECL(name,type) STEAM_GAMESERVER_CALLBACK(GameServerHandler, On##name, type, m_##name); vdynamic *Encode##name( type *t );
#	include "serverevents.h"

#	undef EVENT_IMPL
#	define EVENT_IMPL(name,type) vdynamic *GameServerHandler::Encode##name( type *d )
};

#define EVENT_DECL(name,type) void GameServerHandler::On##name( type *t ) { GlobalEvent(type::k_iCallback, Encode##name(t)); }
#include "serverevents.h"

EVENT_IMPL(ServerConnectFailure, SteamServerConnectFailure_t) {
	HLValue v;
	v.Set("result", d->m_eResult);
	v.Set("stillRetrying", d->m_bStillRetrying);
	return v.value;
}

EVENT_IMPL(ServersConnected, SteamServersConnected_t) {
	return NULL;
}

EVENT_IMPL(ServersDisconnected, SteamServersDisconnected_t) {
	HLValue v;
	v.Set("result", d->m_eResult);
	return v.value;
}

EVENT_IMPL(P2PSessionRequest, P2PSessionRequest_t) {
	HLValue v;
	v.Set("uid", d->m_steamIDRemote);
	return v.value;
}

EVENT_IMPL(P2PSessionConnectFail, P2PSessionConnectFail_t) {
	HLValue v;
	v.Set("uid", d->m_steamIDRemote);
	v.Set("error", d->m_eP2PSessionError);
	return v.value;
}

EVENT_IMPL(PolicyResponse, GSPolicyResponse_t) {
	HLValue v;
	v.Set("secure", d->m_bSecure);
	return v.value;
}

EVENT_IMPL(ValidateAuthTicketResponse, ValidateAuthTicketResponse_t ) {
	HLValue v;
	v.Set("steamId", d->m_SteamID);
	v.Set("ownerSteamId", d->m_OwnerSteamID);
	v.Set("authSessionResponse",d->m_eAuthSessionResponse);
	return v.value;
}

static GameServerHandler *serverHandler = NULL;
static vclosure *globalEvent = NULL;

static void GlobalEvent( int id, vdynamic *v ) {
	vdynamic i;
	vdynamic *args[2];
	i.t = &hlt_i32;
	i.v.i = id;
	args[0] = &i;
	args[1] = v;
	hl_dyn_call(globalEvent, args, 2);
}

HL_PRIM void HL_NAME(gameserver_setup)( vclosure *onGlobalEvent ){
	serverHandler = new GameServerHandler();
	globalEvent = onGlobalEvent;
	hl_add_root(&globalEvent);
}

bool HL_NAME(gameserver_init)( int ip, int port, int gameport, int queryport, int serverMode, char *version ) {
	return SteamGameServer_Init(ip,port,gameport,queryport,(EServerMode)serverMode, version);
}

void HL_NAME(gameserver_runcallbacks)() {
	SteamGameServer_RunCallbacks();
}

void HL_NAME(gameserver_shutdown)() {
	SteamGameServer_Shutdown();
}

void HL_NAME(gameserver_logon_anonymous)() {
	SteamGameServer()->LogOnAnonymous();
}

void HL_NAME(gameserver_enable_heartbeats)( bool b ) {
	SteamGameServer()->EnableHeartbeats(b);
}

void HL_NAME(gameserver_config)( char *modDir, char *product, char *desc ) {
	SteamGameServer()->SetModDir(modDir);
	SteamGameServer()->SetProduct(product);
	SteamGameServer()->SetGameDescription(desc);
}

DEFINE_PRIM(_BOOL, gameserver_init, _I32 _I32 _I32 _I32 _I32 _BYTES);
DEFINE_PRIM(_VOID, gameserver_setup, _FUN(_VOID, _I32 _DYN));
DEFINE_PRIM(_VOID, gameserver_runcallbacks, _NO_ARG);
DEFINE_PRIM(_VOID, gameserver_shutdown, _NO_ARG);
DEFINE_PRIM(_VOID, gameserver_logon_anonymous, _NO_ARG);
DEFINE_PRIM(_VOID, gameserver_enable_heartbeats, _BOOL);
DEFINE_PRIM(_VOID, gameserver_config, _BYTES _BYTES _BYTES);
