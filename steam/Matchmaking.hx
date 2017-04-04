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

			l.onDataUpdated(data.member);
		});
	}

	@:hlNative("steam", "match_init") static function _init() {
	}

	public static function requestLobbyList( onResult : Callback<Int> ) : AsyncCall {
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

}

