package steamwrap.api;
import haxe.io.Bytes;

/**
 * The Steam Cloud API. Used by API.hx, should never be created manually by the user.
 * API.hx creates and initializes this by default.
 * Access it via API.ugcstatic variable
 */

@:allow(steamwrap.api.Steam)
class Cloud
{
	/*************PUBLIC***************/
	
	/**
	 * Whether the Cloud API is initialized or not. If false, all calls will fail.
	 */
	public var active(default, null):Bool = false;
	
	//TODO: these all need documentation headers
	
	public function GetFileCount():Int {
		return SteamWrap_GetFileCount.call(0);
	}
	
	public function FileExists(name:String):Bool {
		return SteamWrap_FileExists.call(name) == 1;
	}
	
	public function GetFileSize(name:String):Int {
		return SteamWrap_GetFileSize.call(name);
	}
	
	public function GetQuota():{total:Int, available:Int}
	{
		var str:String = SteamWrap_GetQuota();
		var arr = str.split(",");
		if (arr != null && arr.length == 2)
		{
			var total = Std.parseInt(arr[0]);
			if (total == null) total = 0;
			var available = Std.parseInt(arr[1]);
			if (available == null) available = 0;
			return {total:total, available:available};
		}
		return {total:0, available:0};
	}
	
	public function FileRead(name:String):String
	{
		if (!FileExists(name))
		{
			return null;
		}
		var fileData:String = SteamWrap_FileRead(name);
		return fileData;
	}
	
	public function FileShare(name:String) {
		SteamWrap_FileShare.call(name);
	}
	
	public function FileWrite(name:String, data:Bytes):Void
	{
		SteamWrap_FileWrite(name, data);
	}
	
	public function FileDelete(name:String):Bool {
		return SteamWrap_FileDelete.call(name) == 1;
	}
	
	public function IsCloudEnabledForApp():Bool {
		return SteamWrap_IsCloudEnabledForApp.call(0) == 1;
	}
	
	public function SetCloudEnabledForApp(b:Bool):Void {
		var i = b ? 1 : 0;
		SteamWrap_SetCloudEnabledForApp.call(i);
	}
	
	/*************PRIVATE***************/
	
	private var customTrace:String->Void;
	private var appId:Int;
	
	//Old-school CFFI calls:
	private var SteamWrap_FileRead:Dynamic;
	private var SteamWrap_FileWrite:Dynamic;
	private var SteamWrap_GetQuota:Dynamic;
	
	//CFFI PRIME calls:
	private var SteamWrap_GetFileCount:Dynamic;
	private var SteamWrap_FileExists:Dynamic;
	private var SteamWrap_FileDelete:Dynamic;
	private var SteamWrap_GetFileSize:Dynamic;
	private var SteamWrap_FileShare:Dynamic;
	private var SteamWrap_IsCloudEnabledForApp:Dynamic;
	private var SteamWrap_SetCloudEnabledForApp:Dynamic;
	
	private function new(appId_:Int, CustomTrace:String->Void) {
		if (active) return;
		
		appId = appId_;
		customTrace = CustomTrace;
		
		active = true;
	}
}