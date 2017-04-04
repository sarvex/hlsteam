package steam;

@:hlNative("steam")
class User {

	public var uid(default, null) : UID;
	public var name(get, never) : String;
	var cachedName : String;

	public function new(uid) {
		this.uid = uid;
	}

	function get_name() {
		if( cachedName != null ) return cachedName;
		cachedName = @:privateAccess String.fromUTF8(get_user_name(uid));
		return cachedName;
	}

	@:keep function __compare( u : User ) : Int {
		return uid.getBytes().compare(u.uid.getBytes());
	}

	public function toString() {
		return name+"(" + uid.toString() + ")";
	}

	static function get_user_name( uid : UID ) : hl.Bytes {
		return null;
	}

}