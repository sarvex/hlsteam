package steam;

@:hlNative("steam")
class Lobby {

	public var uid(default, null) : UID;

	public function new(uid:UID) {
		this.uid = uid;
	}

	public function setData( key : String, value : String ) {
	}

	public function removeData( key : String ) {
	}

	public function getData( key : String ) : String {
		return null;
	}

	public function getAllData() : Map<String,String> {
		var m = new Map();
		return m;
	}

	public function requestData() {
		return request_lobby_data(uid);
	}

	static function request_lobby_data( uid : UID ) {
		return false;
	}

	/*
	static function set_lobby_data( uid : UID, key : hl.Bytes, data : hl.Bytes ) {
	}

	static function remove_lobby_data( uid : UID, key : hl.Bytes ) {
	}

	static function get_lobby_data( uid : UID, key : hl.Bytes ) : hl.Bytes {
		return null;
	}

	static function get_lobby_data_count( uid : UID ) : Int {
		return 0;
	}

	static function get_lobby_data_byindex( uid : UID,  ) : Int {
		return 0;
	}*/

}