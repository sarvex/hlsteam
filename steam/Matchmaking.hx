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

	public static function requestLobbyList( onResult : Callback<Int> ) : AsyncCall {
		return null;
	}

	public static function createLobby( kind : LobbyKind, maxPlayers : Int, onResult : Callback<UID> ) : AsyncCall {
		return null;
	}

}