package steam;
import steam.Api;

/**
 * The User Generated Content API. Used by API.hx, should never be created manually by the user.
 * API.hx creates and initializes this by default.
 * Access it via API.ugcstatic variable
 */

@:allow(steam.Api)
class UGC
{
	/*************PUBLIC***************/

	/**
	 * Whether the UGC API is initialized or not. If false, all calls will fail.
	 */
	public var active(default, null):Bool = false;

	//TODO: these all need documentation headers

	public function createItem():Void {
		_CreateUGCItem(appId);
	}

	public function setItemContent(updateHandle:String, absPath:String):Bool {
		return _SetUGCItemContent(@:privateAccess updateHandle.toUtf8(), @:privateAccess absPath.toUtf8());
	}

	public function setItemDescription(updateHandle:String, itemDesc:String):Bool {
		return _SetUGCItemDescription(@:privateAccess updateHandle.toUtf8(), @:privateAccess itemDesc.substr(0, 8000).toUtf8());
	}

	public function setItemPreviewImage(updateHandle:String, absPath:String):Bool {
		return _SetUGCItemPreviewImage(@:privateAccess updateHandle.toUtf8(), @:privateAccess absPath.toUtf8());
	}

	public function setItemTags(updateHandle:String, tags:String):Bool {
		return _SetUGCItemTags(@:privateAccess updateHandle.toUtf8(), @:privateAccess tags.toUtf8());
	}

	public function addItemKeyValueTag(updateHandle:String, key:String, value:String):Bool {
		return _AddUGCItemKeyValueTag(@:privateAccess updateHandle.toUtf8(), @:privateAccess key.toUtf8(), @:privateAccess value.toUtf8());
	}

	public function removeItemKeyValueTags(updateHandle:String, key:String):Bool {
		return _RemoveUGCItemKeyValueTags(@:privateAccess updateHandle.toUtf8(), @:privateAccess key.toUtf8());
	}

	public function setItemTitle(updateHandle:String, itemTitle:String):Bool {
		return _SetUGCItemTitle(@:privateAccess updateHandle.toUtf8(), @:privateAccess itemTitle.substr(0, 128).toUtf8());
	}

	public function setItemVisibility(updateHandle:String, visibility:Int):Bool {
		/*
		* 	https://partner.steamgames.com/documentation/ugc
		*	0 : Public
		*	1 : Friends Only
		*	2 : Private
		*/
		return _SetUGCItemVisibility(@:privateAccess updateHandle.toUtf8(), visibility);
	}

	public function startUpdateItem(itemID:Int):String {
		return @:privateAccess String.fromUTF8(_StartUpdateUGCItem(appId, itemID));
	}

	public function submitItemUpdate(updateHandle:String, changeNotes:String):Bool {
		return _SubmitUGCItemUpdate(@:privateAccess updateHandle.toUtf8(), @:privateAccess changeNotes.toUtf8());
	}

	public function getNumSubscribedItems():Int {
		return _GetNumSubscribedItems();
	}

	public function getItemState(fileID:String):EItemState {
		var result:EItemState = _GetItemState(@:privateAccess fileID.toUtf8());
		return result;
	}

	/**
	 * Begin downloading a UGC
	 * @param	fileID
	 * @param	highPriority
	 * @return
	 */
	public function downloadItem(fileID:String, highPriority:Bool):Bool {
		return _DownloadItem(@:privateAccess fileID.toUtf8(), highPriority);
	}

	/**
	 * Filter query results to only those that include this tag
	 * @param	queryHandle
	 * @param	tagName
	 * @return
	 */
	public function addRequiredTag(queryHandle:String, tagName:String):Bool {
		return _AddRequiredTag(@:privateAccess queryHandle.toUtf8(), @:privateAccess tagName.toUtf8());
	}

	/**
	 * Filter query results to only those that lack this tag
	 * @param	queryHandle
	 * @param	tagName
	 * @return
	 */
	public function addExcludedTag(queryHandle:String, tagName:String):Bool {
		return _AddExcludedTag(@:privateAccess queryHandle.toUtf8(), @:privateAccess tagName.toUtf8());
	}

	/**
	 * Filter query results to only those that have a key with name `key` and a value that matches `value`
	 * @param	queryHandle
	 * @param	key
	 * @param	value
	 * @return
	 */
	public function addRequiredKeyValueTag(queryHandle:String, key:String, value:String):Bool {
		return _AddRequiredKeyValueTag(@:privateAccess queryHandle.toUtf8(), @:privateAccess key.toUtf8(), @:privateAccess value.toUtf8());
	}

	/**
	 * Return any key-value tags for the items
	 * @param	queryHandle
	 * @param	returnKeyValueTags
	 * @return
	 */
	public function setReturnKeyValueTags(queryHandle:String, returnKeyValueTags:Bool):Bool {
		return _SetReturnKeyValueTags(@:privateAccess queryHandle.toUtf8(), returnKeyValueTags);
	}

	/**
	 * Return the developer specified metadata for each queried item
	 * @param	queryHandle
	 * @param	returnMetadata
	 * @return
	 */
	public function setReturnMetadata(queryHandle:String, returnMetadata:Bool):Bool {
		return _SetReturnMetadata(@:privateAccess queryHandle.toUtf8(), returnMetadata);
	}

	/**
	 * Get an array of publishedFileID's for UGC items the user is subscribed to
	 * @return publishedFileID array (represented as strings)
	 */
	public function getSubscribedItems():Array<String>{
		var result = @:privateAccess String.fromUTF8(_GetSubscribedItems());
		if (result == "" || result == null) return [];
		var arr:Array<String> = result.split(",");
		return arr;
	}

	/**
	 * Get the amount of bytes that have been downloaded for the given file
	 * @param	fileID the publishedFileID for this UGC item
	 * @return	an array: [0] is bytesDownloaded, [1] is bytesTotal
	 */
	public function getItemDownloadInfo(fileID:String):Array<Float>{
		var ai = 0., bi = 0.;
		_GetItemDownloadInfo(@:privateAccess fileID.toUtf8(), ai, bi);
		return [ai, bi];
	}

	public function getItemInstallInfo(fileID:String):GetItemInstallInfoResult{
		var result = _GetItemInstallInfo(@:privateAccess fileID.toUtf8(), 30000);
		return GetItemInstallInfoResult.fromString(@:privateAccess String.fromUTF8(result));
	}

	/*
	 * Query UGC associated with a user. Creator app id or consumer app id must be valid and be set to the current running app. Page should start at 1.
	 */
	/*
	public function createQueryUserUGCRequest(accountID:String, listType, matchingUGCType, sortOrder, creatorAppID:String, consumerAppID:String, page:Int)
	{

	}
	*/

	/**
	 * Query for all matching UGC. Creator app id or consumer app id must be valid and be set to the current running app. unPage should start at 1.
	 * @param	queryType
	 * @param	matchingUGCType
	 * @param	creatorAppID
	 * @param	consumerAppID
	 * @param	page
	 * @return
	 */
	public function createQueryAllUGCRequest(queryType:EUGCQuery, matchingUGCType:EUGCMatchingUGCType, creatorAppID:Int, consumerAppID:Int, page:Int):String
	{
		var result:String = @:privateAccess String.fromUTF8(_CreateQueryAllUGCRequest(queryType, matchingUGCType, creatorAppID, consumerAppID, page));
		return result;
	}

	/**
	 * Query for the details of the given published file ids
	 * @param	fileIDs
	 * @return
	 */
	public function createQueryUGCDetailsRequest(fileIDs:Array<String>):String
	{
		var result = @:privateAccess String.fromUTF8(_CreateQueryUGCDetailsRequest(@:privateAccess fileIDs.join(",").toUtf8()));
		return result;
	}

	/**
	 * Send the query to Steam
	 * @param	handle
	 */
	public function sendQueryUGCRequest(handle:String):Void
	{
		trace("sendQueryUGCRequest(" + handle+")");
		_SendQueryUGCRequest(@:privateAccess handle.toUtf8());
	}

	/**
	 * Retrieve an individual result after receiving the callback for querying UGC
	 * @param	handle
	 * @param	index
	 * @return
	 */
	public function getQueryUGCResult(handle:String, index:Int):SteamUGCDetails
	{
		var result:String = @:privateAccess String.fromUTF8(_GetQueryUGCResult(@:privateAccess handle.toUtf8(), index));
		var details:SteamUGCDetails = SteamUGCDetails.fromString(result);
		return details;
	}

	/**
	 *
	 * @param	handle
	 * @param	index
	 * @return
	 */
	public function getQueryUGCMetadata(handle:String, index:Int):String
	{
		var result:String = @:privateAccess String.fromUTF8(_GetQueryUGCMetadata(@:privateAccess handle.toUtf8(), index, 5000));
		return result;
	}

	public function getQueryUGCNumKeyValueTags(handle:String, index:Int):Int
	{
		return _GetQueryUGCNumKeyValueTags(@:privateAccess handle.toUtf8(), index);
	}

	public function getQueryUGCKeyValueTag(handle:String, index:Int, keyValueTagIndex:Int):Array<String>
	{
		var result:String = @:privateAccess String.fromUTF8(_GetQueryUGCKeyValueTag(@:privateAccess handle.toUtf8(), index, keyValueTagIndex, 255, 255));
		if (result != null && result.indexOf("=") != -1){
			var arr = result.split("=");
			if (arr != null && arr.length == 2 && arr[0] != null && arr[1] != null){
				return arr;
			}
		}
		return ["",""];
	}

	public function releaseQueryUGCRequest(handle:String):Bool
	{
		return _ReleaseQueryUGCRequest(@:privateAccess handle.toUtf8());
	}

	/*************PRIVATE***************/

	private var customTrace:String->Void;
	private var appId:Int;

	@:hlNative("steam","create_ugc_item") private static function _CreateUGCItem( id : Int ) : Bool { return false; }
	@:hlNative("steam","set_ugc_item_title") private static function _SetUGCItemTitle( h : hl.Bytes, title : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","set_ugc_item_tags") private static function _SetUGCItemTags( h : hl.Bytes, tags : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","add_ugc_item_key_value_tag") private static function _AddUGCItemKeyValueTag( h : hl.Bytes, key : hl.Bytes, value : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","remove_ugc_item_key_value_tags") private static function _RemoveUGCItemKeyValueTags( h: hl.Bytes, key : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","set_ugc_item_description") private static function _SetUGCItemDescription( h : hl.Bytes, desc : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","set_ugc_item_visibility") private static function _SetUGCItemVisibility( h : hl.Bytes, visibility : Int ) : Bool { return false; }
	@:hlNative("steam","set_ugc_item_content") private static function _SetUGCItemContent( h : hl.Bytes, path : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","set_ugc_item_preview_image") private static function _SetUGCItemPreviewImage( h : hl.Bytes, path : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","start_update_ugc_item") private static function _StartUpdateUGCItem( id : Int, item : Int ) : hl.Bytes { return null; }
	@:hlNative("steam","submit_ugc_item_update") private static function _SubmitUGCItemUpdate( h : hl.Bytes, changeNotes : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","get_subscribed_items") private static function _GetSubscribedItems() : hl.Bytes { return null; }
	@:hlNative("steam","get_item_download_info") private static function _GetItemDownloadInfo( publishedFileID : hl.Bytes, downloaded : hl.Ref<Float>, total : hl.Ref<Float> ) : Bool{ return false; }
	@:hlNative("steam","get_item_install_info") private static function _GetItemInstallInfo( publishedFileID : hl.Bytes, maxFolderPathLength : Int ) : hl.Bytes { return null; }
	@:hlNative("steam","create_query_all_ugc_request") private static function _CreateQueryAllUGCRequest( queryType : Int, matchingUGCType : Int, creatorAppID : Int, consumerAppID : Int, page : Int ) : hl.Bytes { return null; }
	@:hlNative("steam","create_query_ugc_details_request") private static function _CreateQueryUGCDetailsRequest( fileIDs : hl.Bytes ) : hl.Bytes { return null; }
	@:hlNative("steam","get_query_ugc_result") private static function _GetQueryUGCResult( h : hl.Bytes, index : Int ) : hl.Bytes { return null; }
	@:hlNative("steam","get_query_ugc_key_value_tag") private static function _GetQueryUGCKeyValueTag( h : hl.Bytes, index : Int, keyValueTagIndex : Int, keySize : Int, valueSize : Int ) : hl.Bytes { return null; }
	@:hlNative("steam","get_query_ugc_metadata") private static function _GetQueryUGCMetadata( h : hl.Bytes, index : Int, metadataSize : Int ) : hl.Bytes { return null; }
	@:hlNative("steam","get_num_subscribed_items") private static function _GetNumSubscribedItems() : Int { return 0; }
	@:hlNative("steam","get_item_state") private static function _GetItemState( publishedFileID : hl.Bytes ) : Int { return 0; }
	@:hlNative("steam","download_item") private static function _DownloadItem( publishedFileID : hl.Bytes, highPriority : Bool ) : Bool { return false; }
	@:hlNative("steam","add_required_key_value_tag") private static function _AddRequiredKeyValueTag( h : hl.Bytes, key : hl.Bytes, value : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","add_required_tag") private static function _AddRequiredTag( h : hl.Bytes, tag : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","add_excluded_tag") private static function _AddExcludedTag( h : hl.Bytes, tag : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","send_query_ugc_request") private static function _SendQueryUGCRequest( h : hl.Bytes ): Void{};
	@:hlNative("steam","set_return_metadata") private static function _SetReturnMetadata( h : hl.Bytes, returnMetadata : Bool ) : Bool { return false; }
	@:hlNative("steam","set_return_key_value_tags") private static function _SetReturnKeyValueTags( h : hl.Bytes, returnKeyValueTags : Bool ) : Bool { return false; }
	@:hlNative("steam","release_query_ugc_request") private static function _ReleaseQueryUGCRequest( h : hl.Bytes ) : Bool { return false; }
	@:hlNative("steam","get_query_ugc_num_key_value_tags") private static function _GetQueryUGCNumKeyValueTags( h : hl.Bytes, index : Int ) : Int { return 0; }

	private function new(appId_:Int, CustomTrace:String->Void) {
		#if sys		//TODO: figure out what targets this will & won't work with and upate this guard

		if (active) return;

		appId = appId_;
		customTrace = CustomTrace;


		// if we get this far, the dlls loaded ok and we need Steam controllers to init.
		// otherwise, we're trying to run the Steam version without the Steam client
		active = true;//_InitControllers();

		#end
	}
}