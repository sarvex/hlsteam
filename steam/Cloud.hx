package steam;
import haxe.io.Bytes;

/**
 * The Steam Cloud API. Used by API.hx, should never be created manually by the user.
 * API.hx creates and initializes this by default.
 * Access it via API.ugcstatic variable
 */

@:allow(steam.Api)
class Cloud
{
	/*************PUBLIC***************/

	/**
	 * Whether the Cloud API is initialized or not. If false, all calls will fail.
	 */
	public var active(default, null):Bool = false;

	//TODO: these all need documentation headers

	public function GetFileCount():Int {
		return _GetFileCount();
	}

	public function FileExists(name:String):Bool {
		return _FileExists(@:privateAccess name.toUtf8());
	}

	public function GetFileSize(name:String):Int {
		return _GetFileSize(@:privateAccess name.toUtf8());
	}

	public function GetQuota():{total:Float, available:Float}
	{
		var total = 0., available = 0.;
		_GetQuota(total, available);
		return {total:total, available:available};
	}

	public function FileRead(name:String):haxe.io.Bytes
	{
		if (!FileExists(name))
			return null;
		var len = 0;
		var fileData:hl.Bytes = _FileRead(@:privateAccess name.toUtf8(), len);
		if( fileData == null ) return null;
		return @:privateAccess new haxe.io.Bytes(fileData, len);
	}

	public function FileShare(name:String) {
		_FileShare(@:privateAccess name.toUtf8());
	}

	public function FileWrite(name:String, data:Bytes):Void
	{
		_FileWrite(@:privateAccess name.toUtf8(), data.getData(), data.length);
	}

	public function FileDelete(name:String):Bool {
		return _FileDelete(@:privateAccess name.toUtf8());
	}

	public function IsCloudEnabledForApp():Bool {
		return _IsCloudEnabledForApp();
	}

	public function SetCloudEnabledForApp(b:Bool):Void {
		_SetCloudEnabledForApp(b);
	}

	/*************PRIVATE***************/

	private var customTrace:String->Void;
	private var appId:Int;

	@:hlNative("steam","file_read") private static function _FileRead( name : hl.Bytes, len : hl.Ref<Int> ) : hl.Bytes { return null; }
	@:hlNative("steam","file_write") private static function _FileWrite( name : hl.Bytes, bytes : hl.Bytes, len : Int ) : Bool { return false; }
	@:hlNative("steam","get_quota") private static function _GetQuota( total : hl.Ref<Float>, available : hl.Ref<Float> ): Void{};
	@:hlNative("steam","get_file_count") private static function _GetFileCount() : Int { return 0; }
	@:hlNative("steam","file_exists") private static function _FileExists( name : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","file_delete") private static function _FileDelete( name : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","get_file_size") private static function _GetFileSize( name : hl.Bytes ) : Int { return 0; }
	@:hlNative("steam","file_share") private static function _FileShare( name : hl.Bytes ): Void{};
	@:hlNative("steam","is_cloud_enabled_for_app") private static function _IsCloudEnabledForApp() : Bool { return false; }
	@:hlNative("steam","set_cloud_enabled_for_app") private static function _SetCloudEnabledForApp( enabled : Bool ) : Void {};

	private function new(appId_:Int, CustomTrace:String->Void) {
		if (active) return;

		appId = appId_;
		customTrace = CustomTrace;

		active = true;
	}
}