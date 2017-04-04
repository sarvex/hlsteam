package steam;

@:hlNative("steam")
@:access(String)
class Lobby {

	public var uid(default, null) : UID;

	public function new(uid:UID) {
		this.uid = uid;
	}

	public function setData( key : String, value : String ) {
		set_lobby_data(uid, key.toUtf8(), value.toUtf8());
	}

	public function removeData( key : String ) {
		delete_lobby_data(uid, key.toUtf8());
	}

	public function getData( key : String ) : String {
		var v = get_lobby_data(uid, key.toUtf8());
		return v == null ? null : String.fromUTF8(v);
	}

	public function getAllData() : Map<String,String> {
		var m = new Map();
		for( i in 0...get_lobby_data_count(uid) ) {
			trace(i);
		}
		return m;
	}

	public function requestData() {
		register();
		return request_lobby_data(uid);
	}

	function register() {
		@:privateAccess Matchmaking.lobbies.set(uid.toString(), this);
	}

	public dynamic function onDataUpdated( user : Null<UID> ) {
	}

	public function dispose() {
		@:privateAccess Matchmaking.lobbies.remove(uid.toString());
	}

	static function request_lobby_data( uid : UID ) {
		return false;
	}

	static function set_lobby_data( uid : UID, key : hl.Bytes, data : hl.Bytes ) {
	}

	static function delete_lobby_data( uid : UID, key : hl.Bytes ) {
	}

	static function get_lobby_data( uid : UID, key : hl.Bytes ) : hl.Bytes {
		return null;
	}

	static function get_lobby_data_count( uid : UID ) : Int {
		return 0;
	}

}