package steam;

@:enum abstract ServerMode(Int) {
	var NoAuthentification = 1;
	var Authentification = 2;
	var AuthentificationAndSecure = 3;
}

@:hlNative("steam")
class GameServer {

	static var initDone = false;
	static var globalEvents = new Map<Int,Dynamic->Void>();

	@:noComplete public static function registerGlobalEvent( event : Int, callb : Dynamic -> Void ) {
		globalEvents.set(event, callb);
	}

	public static function init( ip : sys.net.Host, port : Int, gameport : Int, queryPort : Int, serverMode : ServerMode, version : String ) {
		var ip = ip.ip;
		var convIP = (ip >>> 24) | ((ip >> 8) & 0xFF00) | ((ip << 8) & 0xFF0000) | (ip << 24);
		if( !gameserver_init(convIP, port, gameport, queryPort, serverMode, @:privateAccess version.toUtf8()) )
			return false;
		if( initDone ) return true;
		haxe.MainLoop.add(runGameServer);
		gameserver_setup(onGlobalEvent);
		gameserver_config( @:privateAccess "".toUtf8(), @:privateAccess "Test Server".toUtf8(), @:privateAccess "desc".toUtf8() );
		return true;
	}

	static function onGlobalEvent( event : Int, data : Dynamic ) {
		var callb = globalEvents.get(event);
		if( callb != null )
			callb(data);
		else
			customTrace("Unhandled server event #"+event+" ("+data+")");
	}

	public static dynamic function customTrace(str:String) {
		Sys.println(str);
	}

	@:hlNative("steam", "gameserver_enable_heartbeats")
	public static function enableHeartbeats( b : Bool ) {
	}

	public static function logonAnonymous( onLogin : Bool -> Void ) {

		var ucb = 100;
		//SteamServersConnected_t
		registerGlobalEvent(ucb + 1, function(_) onLogin(true));
		//SteamServerConnectFailure_t
		registerGlobalEvent(ucb + 2, function(r) { customTrace("CONNECT FAILURE " + r); onLogin(false); });
		//SteamServersDisconnected_t
		registerGlobalEvent(ucb + 3, function(r) { customTrace("DISCONNECTED " + r); onLogin(false); });

		registerGlobalEvent(ucb + 15, function(_) { /* ignore VAC flag */ });

		gameserver_logon_anonymous();
	}

	@:hlNative("steam", "gameserver_shutdown")
	public static function shutdown() {
	}

	static function runGameServer() {
		gameserver_runcallbacks();
	}

	static function gameserver_init( ip : Int, port : Int, gameport : Int, queryport : Int, serverMode : ServerMode, version : hl.Bytes ) : Bool {
		return false;
	}

	static function gameserver_runcallbacks() {
	}

	static function gameserver_logon_anonymous() {
	}

	static function gameserver_setup( onGlobalEvent : Int -> Dynamic -> Void ) {
	}

	static function gameserver_config( modDir : hl.Bytes, name : hl.Bytes, desc : hl.Bytes ) {
	}

}