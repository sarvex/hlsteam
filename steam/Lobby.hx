package steam;

@:hlNative("steam")
@:access(String)
class Lobby {

	public var uid(default, null) : UID;
	public var owner(get, never) : User;

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

	public function getAllData( maxSize = 65536 ) : Map<String,String> {
		var m = new Map();
		var tmpKey = new hl.Bytes(256);
		var tmpData = new hl.Bytes(maxSize);
		for( i in 0...get_lobby_data_count(uid) ) {
			if( !get_lobby_data_byindex(uid, i, tmpKey, 256, tmpData, maxSize) )
				throw "Too much data";
			m.set(String.fromUTF8(tmpKey), String.fromUTF8(tmpData));
		}
		return m;
	}

	public function leave() {
		dispose();
		leave_lobby(uid);
	}

	function get_owner() {
		return new User(get_lobby_owner(uid));
	}

	public function join( onJoin : { inviteOnly : Bool } -> Void ) {
		return join_lobby(uid, function(v,error) onJoin(error ? null : { inviteOnly : v }));
	}

	public function getMembers() {
		return [for( i in 0...get_num_lobby_members(uid) ) new User(get_lobby_member_by_index(uid, i))];
	}

	public function requestData() {
		register();
		return request_lobby_data(uid);
	}

	function register() {
		@:privateAccess Matchmaking.lobbies.set(uid.toString(), this);
	}

	public dynamic function onDataUpdated() {
	}

	public dynamic function onUserDataUpdated( uid : UID ) {
	}

	public function inviteFriends() {
		lobby_invite_friends(uid);
	}

	public function toString() {
		return "Lobby#" + uid.toString();
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

	static function get_lobby_data_byindex( uid : UID, index : Int, key : hl.Bytes, keySize : Int, data : hl.Bytes, dataSize : Int ) {
		return false;
	}

	static function leave_lobby( uid : UID ) {
	}

	static function join_lobby( uid : UID, onJoin : Callback<Bool> ) : AsyncCall {
		return null;
	}

	static function get_num_lobby_members( uid : UID ) : Int {
		return 0;
	}

	static function get_lobby_member_by_index( uid : UID, index : Int ) : UID {
		return null;
	}

	static function get_lobby_owner( uid : UID ) : UID {
		return null;
	}

	static function lobby_invite_friends( uid : UID ) {
	}

}