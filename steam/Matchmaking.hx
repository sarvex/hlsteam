package steam;

@:enum abstract LobbyKind(Int) {
	/**
		Only way to join the lobby is to invite to someone else
	**/
	public var Private = 0;
	/**
		Shows for friends or invitees, but not in lobby list
	**/
	public var FriendsOnly = 1;
	/**
		Visible for friends and in lobby list
	**/
	public var Public = 2;
	/**
		Returned by search, but not visible to other friends
	**/
	public var Invisible = 3;
}

private enum LobbyChatFlag {
	Entered;
	Left;
	Disconnected;
	Kicked;
	Banned;
}

@:hlNative("steam")
class Matchmaking {

	static var lobbies = new Map<String,Lobby>();
	static var _init = init();

	static function init() {
		var eid = 500;
		var fid = 300;

		// LobbyDataUpdate_t
		Api.registerGlobalEvent(eid + 5, function(data:{lobby:UID, user:Null<UID>}) {
			if( data == null ) {
				Api.customTrace("Failed to retreive data");
				return;
			}
			var l = getLobby(data.lobby);
			if( l == null ) return;
			if( data.user != null )
				l.onUserDataUpdated(new User(data.user));
			else
				l.onDataUpdated();
		});

		// LobbyChatUpdate_t
		Api.registerGlobalEvent(eid + 6, function(data:{lobby:UID, user:UID, origin:UID, flags:haxe.EnumFlags<LobbyChatFlag>}) {
			var l = getLobby(data.lobby);
			if( l == null ) return;
			if( data.flags.has(Entered) )
				l.onUserJoined(new User(data.user));
			else
				l.onUserLeft(new User(data.user)); // no need for reason for now
		});

		// LobbyChatMsg_t
		Api.registerGlobalEvent(eid + 7, function(data:{lobby:UID, user:UID, type:Lobby.ChatMessageType, cid:Int}) {
			var l = getLobby(data.lobby);
			if( l == null ) return;
			l.onChatMessage(data.type, data.cid);
		});

		// PersonaStateChange_t
		Api.registerGlobalEvent(fid + 4, function(data:{user:UID, flags:Int}) {
			for( l in lobbies ) {
				for( u in l.getMembers() )
					if( u.uid == data.user ) {
						l.onUserDataUpdated(u);
						break;
					}
			}
		});

		// GameLobbyJoinRequested_t
		Api.registerGlobalEvent(fid + 33, function(data:{user:UID, lobby:UID}) {
			var l = lobbies.get(data.lobby.toString());
			if( l == null )
				l = new Lobby(data.lobby);
			onInvited(l, new User(data.user));
		});

		return true;
	}

	static function getLobby( lid : UID ) {
		var l = lobbies.get(lid.toString());
		if( l == null )
			Api.customTrace("Lobby not found " + lid);
		return l;
	}

	public static function requestLobbyList( onLobbyList : Array<Lobby> -> Void ) {
		request_lobby_list(function(count, error) {
			if( error ) {
				onLobbyList(null);
				return;
			}
			onLobbyList([for( i in 0...count ) new Lobby(get_lobby_by_index(i))]);
		});
	}

	static function request_lobby_list( onResult : Callback<Int> ) : AsyncCall {
		return null;
	}

	public static function createLobby( kind : LobbyKind, maxPlayers : Int, onResult : Lobby -> Void ) : AsyncCall {
		return create_lobby(kind, maxPlayers, function(uid, error) {
			if( error ) {
				onResult(null);
				return;
			}
			var l = new Lobby(uid);
			@:privateAccess l.register();
			onResult(l);
		});
	}

	public static dynamic function onInvited( lobby : Lobby, by : User ) {
		Api.customTrace("onInvited not handled for " + lobby + " by " + by);
	}

	public static function checkInvite( onInvite : Lobby -> Void ) {
		var args = Sys.args();
		if( args[0] != "+connect_lobb" )
			return;
		var uid = haxe.Int64.parseString(args[1]);
		var bytes = new hl.Bytes(8);
		bytes.setI32(0, uid.low);
		bytes.setI32(4, uid.high);
		var uid : UID = cast bytes;
		onInvite(new Lobby(uid));
	}

	static function create_lobby( kind : LobbyKind, maxPlayers : Int, onResult : Callback<UID> ) : AsyncCall {
		return null;
	}

	static function get_lobby_by_index( index : Int ) : UID {
		return null;
	}

}

