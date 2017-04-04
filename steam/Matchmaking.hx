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

@:hlNative("steam")
class Matchmaking {

	static var lobbies = new Map<String,Lobby>();

	static function init() {
		_init();
		var eid = 500;
		// LobbyDataUpdate_t
		Api.registerGlobalEvent(eid + 5, function(data:{lobby:UID, member:Null<UID>}) {

			if( data == null ) {
				Api.customTrace("Failed to retreive data");
				return;
			}

			var l = lobbies.get(data.lobby.toString());
			if( l == null ) {
				Api.customTrace("Lobby not found " + data.lobby);
				return;
			}

			if( data.member != null )
				l.onUserDataUpdated(data.member);
			else
				l.onDataUpdated();
		});
	}

	@:hlNative("steam", "match_init") static function _init() {
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

	static function create_lobby( kind : LobbyKind, maxPlayers : Int, onResult : Callback<UID> ) : AsyncCall {
		return null;
	}

	static function get_lobby_by_index( index : Int ) : UID {
		return null;
	}

}

