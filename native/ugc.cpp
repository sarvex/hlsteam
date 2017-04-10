#include "steamwrap.h"

void CallbackHandler::OnDownloadItem(DownloadItemResult_t *pCallback) {
	if (pCallback->m_unAppID != SteamUtils()->GetAppID()) return;

	std::ostringstream fileIDStream;
	PublishedFileId_t m_ugcFileID = pCallback->m_nPublishedFileId;
	fileIDStream << m_ugcFileID;
	SendEvent(ItemDownloaded, pCallback->m_eResult == k_EResultOK, fileIDStream.str().c_str());
}

void CallbackHandler::OnItemInstalled(ItemInstalled_t *pCallback) {
	if (pCallback->m_unAppID != SteamUtils()->GetAppID()) return;

	std::ostringstream fileIDStream;
	PublishedFileId_t m_ugcFileID = pCallback->m_nPublishedFileId;
	fileIDStream << m_ugcFileID;
	SendEvent(ItemDownloaded, true, fileIDStream.str().c_str());
}

void CallbackHandler::SendQueryUGCRequest(UGCQueryHandle_t handle){
	SteamAPICall_t hSteamAPICall = SteamUGC()->SendQueryUGCRequest(handle);
	m_callResultUGCQueryCompleted.Set(hSteamAPICall, this, &CallbackHandler::OnUGCQueryCompleted);
}

void CallbackHandler::OnUGCQueryCompleted(SteamUGCQueryCompleted_t *pCallback, bool biOFailure){
	if (pCallback->m_eResult == k_EResultOK)	{
		std::ostringstream data;
		data << pCallback->m_handle << ",";
		data << pCallback->m_unNumResultsReturned << ",";
		data << pCallback->m_unTotalMatchingResults << ",";
		data << pCallback->m_bCachedData;

		SendEvent(UGCQueryCompleted, true, data.str().c_str());
	}else{
		SendEvent(UGCQueryCompleted, false, NULL);
	}
}

void CallbackHandler::SubmitUGCItemUpdate(UGCUpdateHandle_t handle, const char *pchChangeNote){
	SteamAPICall_t hSteamAPICall = SteamUGC()->SubmitItemUpdate(handle, pchChangeNote);
	m_callResultSubmitUGCItemUpdate.Set(hSteamAPICall, this, &CallbackHandler::OnItemUpdateSubmitted);
}

void CallbackHandler::OnItemUpdateSubmitted(SubmitItemUpdateResult_t *pCallback, bool bIOFailure)
{
	if(	pCallback->m_eResult == k_EResultInsufficientPrivilege ||
		pCallback->m_eResult == k_EResultTimeout ||
		pCallback->m_eResult == k_EResultNotLoggedOn ||
		bIOFailure
	)
		SendEvent(UGCItemUpdateSubmitted, false, NULL);
	else
		SendEvent(UGCItemUpdateSubmitted, true, NULL);
}

void CallbackHandler::CreateUGCItem(AppId_t nConsumerAppId, EWorkshopFileType eFileType){
	SteamAPICall_t hSteamAPICall = SteamUGC()->CreateItem(nConsumerAppId, eFileType);
	m_callResultCreateUGCItem.Set(hSteamAPICall, this, &CallbackHandler::OnUGCItemCreated);
}

void CallbackHandler::OnUGCItemCreated(CreateItemResult_t *pCallback, bool bIOFailure){
	if (bIOFailure)	{
		SendEvent(UGCItemCreated, false, NULL);
		return;
	}

	PublishedFileId_t m_ugcFileID = pCallback->m_nPublishedFileId;

	/*
	*  k_EResultInsufficientPrivilege : The user creating the item is currently banned in the community.
	*  k_EResultTimeout : The operation took longer than expected, have the user retry the create process.
	*  k_EResultNotLoggedOn : The user is not currently logged into Steam.
	*/
	if(	pCallback->m_eResult == k_EResultInsufficientPrivilege ||
		pCallback->m_eResult == k_EResultTimeout ||
		pCallback->m_eResult == k_EResultNotLoggedOn){
		SendEvent(UGCItemCreated, false, NULL);
	}else{
		std::ostringstream fileIDStream;
		fileIDStream << m_ugcFileID;
		SendEvent(UGCItemCreated, true, fileIDStream.str().c_str());
	}

	SendEvent(UGCLegalAgreementStatus, !pCallback->m_bUserNeedsToAcceptWorkshopLegalAgreement, NULL);

	if(pCallback->m_bUserNeedsToAcceptWorkshopLegalAgreement){
		std::ostringstream urlStream;
		urlStream << "steam://url/CommunityFilePage/" << m_ugcFileID;

		// TODO: Separate this to it's own call through wrapper.
		SteamFriends()->ActivateGameOverlayToWebPage(urlStream.str().c_str());
	}
}


//generates a uint64 array from comma-delimeted-values
uint64 * getUint64Array(const char * str, uint32 * numElements){
	std::string stdStr = str;

	//NOTE: this will probably fail if the string includes Unicode, but Steam tags probably don't support that?
	std::vector<std::string> v;
	split(stdStr, ',', v);

	int count = v.size();

	uint64 * values = new uint64[count];

	for(int i = 0; i < count; i++) {
		values[i] = strtoull(v[i].c_str(), NULL, 0);
	}

	*numElements = count;

	return values;
}

HL_PRIM int HL_NAME(get_num_subscribed_items)() {
	if (!CheckInit()) return 0;
	return SteamUGC()->GetNumSubscribedItems();
}
DEFINE_PRIM(_I32, get_num_subscribed_items, _NO_ARG);

HL_PRIM vbyte *HL_NAME(get_subscribed_items)(){
	if (!CheckInit()) return (vbyte*)"";

	int numSubscribed = SteamUGC()->GetNumSubscribedItems();
	if(numSubscribed <= 0) return (vbyte*)"";
	PublishedFileId_t* pvecPublishedFileID = new PublishedFileId_t[numSubscribed];

	int result = SteamUGC()->GetSubscribedItems(pvecPublishedFileID, numSubscribed);

	std::ostringstream data;
	for(int i = 0; i < result; i++){
		if(i != 0)
			data << ",";
		data << pvecPublishedFileID[i];
	}
	delete pvecPublishedFileID;

	return (vbyte*)data.str().c_str();
}
DEFINE_PRIM(_BYTES, get_subscribed_items, _NO_ARG);

HL_PRIM int HL_NAME(get_item_state)(vbyte *publishedFileID){
	if (!CheckInit()) return 0;
	PublishedFileId_t nPublishedFileID = (PublishedFileId_t) strtoll((char*)publishedFileID, NULL, 10);
	return SteamUGC()->GetItemState(nPublishedFileID);
}
DEFINE_PRIM(_I32, get_item_state, _BYTES);

HL_PRIM bool HL_NAME(get_item_download_info)(vbyte *publishedFileID, double *downloaded, double *total ){
	if (!CheckInit()) return (vbyte*)"";

	PublishedFileId_t nPublishedFileID = (PublishedFileId_t) strtoll((char*)publishedFileID, NULL, 10);

	uint64 punBytesDownloaded;
	uint64 punBytesTotal;

	bool result = SteamUGC()->GetItemDownloadInfo(nPublishedFileID, &punBytesDownloaded, &punBytesTotal);

	if (!result)
		return false;

	*downloaded = (double)punBytesDownloaded;
	*total = (double)punBytesTotal;

	return result;
}
DEFINE_PRIM(_BOOL, get_item_download_info, _BYTES _REF(_F64) _REF(_F64));

HL_PRIM bool HL_NAME(download_item)(vbyte *publishedFileID, bool highPriority){
	if (!CheckInit()) return false;
	PublishedFileId_t nPublishedFileID = (PublishedFileId_t) strtoll((char*)publishedFileID, NULL, 10);

	return SteamUGC()->DownloadItem(nPublishedFileID, highPriority);
}
DEFINE_PRIM(_BOOL, download_item, _BYTES _BOOL);

HL_PRIM vbyte *HL_NAME(get_item_install_info)(vbyte *publishedFileID, int maxFolderPathLength){
	if (!CheckInit()) return (vbyte*)"";

	PublishedFileId_t nPublishedFileID = (PublishedFileId_t) strtoll((char*)publishedFileID, NULL, 10);

	uint64 punSizeOnDisk;
	uint32 punTimeStamp;
	uint32 cchFolderSize = (uint32) maxFolderPathLength;
	char * pchFolder = new char[cchFolderSize];

	bool result = SteamUGC()->GetItemInstallInfo(nPublishedFileID, &punSizeOnDisk, pchFolder, cchFolderSize, &punTimeStamp);

	if(result){
		std::ostringstream data;
		data << punSizeOnDisk;
		data << "|";
		data << pchFolder;
		data << "|";
		data << cchFolderSize;
		data << "|";
		data << punTimeStamp;
		return (vbyte*)data.str().c_str();
	}

	return (vbyte*)"0||0|";
}
DEFINE_PRIM(_BYTES, get_item_install_info, _BYTES _I32);

HL_PRIM vbyte *HL_NAME(create_query_all_ugc_request)(int queryType, int matchingUGCType, int creatorAppID, int consumerAppID, int page){
	if (!CheckInit()) return (vbyte*)"";

	EUGCQuery eQueryType = (EUGCQuery) queryType;
	EUGCMatchingUGCType eMatchingUGCType = (EUGCMatchingUGCType) matchingUGCType;
	AppId_t nCreatorAppID = creatorAppID;
	AppId_t nConsumerAppID = consumerAppID;
	uint32 unPage = page;

	UGCQueryHandle_t result = SteamUGC()->CreateQueryAllUGCRequest(eQueryType, eMatchingUGCType, nCreatorAppID, nConsumerAppID, unPage);

	std::ostringstream data;
	data << result;
	return (vbyte*)data.str().c_str();
}
DEFINE_PRIM(_BYTES, create_query_all_ugc_request, _I32 _I32 _I32 _I32 _I32);

HL_PRIM vbyte *HL_NAME(create_query_ugc_details_request)(vbyte *fileIDs){
	if (!CheckInit()) return (vbyte*)"";
	uint32 unNumPublishedFileIDs = 0;
	PublishedFileId_t * pvecPublishedFileID = getUint64Array((char*)fileIDs, &unNumPublishedFileIDs);

	UGCQueryHandle_t result = SteamUGC()->CreateQueryUGCDetailsRequest(pvecPublishedFileID, unNumPublishedFileIDs);

	std::ostringstream data;
	data << result;
	return (vbyte*)data.str().c_str();
}
DEFINE_PRIM(_BYTES, create_query_ugc_details_request, _BYTES);

HL_PRIM void HL_NAME(send_query_ugc_request)(vbyte *cHandle){
	if (!CheckInit()) return;
	UGCQueryHandle_t handle = strtoull((char*)cHandle, NULL, 0);
	s_callbackHandler->SendQueryUGCRequest(handle);
}
DEFINE_PRIM(_VOID, send_query_ugc_request, _BYTES);


HL_PRIM int HL_NAME(get_query_ugc_num_key_value_tags)(vbyte *cHandle, int iIndex){
	if (!CheckInit()) return 0;
	UGCQueryHandle_t handle = strtoull((char*)cHandle, NULL, 0);
	uint32 index = iIndex;
	return SteamUGC()->GetQueryUGCNumKeyValueTags(handle, index);
}
DEFINE_PRIM(_I32, get_query_ugc_num_key_value_tags, _BYTES _I32);

HL_PRIM bool HL_NAME(release_query_ugc_request)(vbyte *cHandle){
	if (!CheckInit()) return false;
	UGCQueryHandle_t handle = strtoull((char*)cHandle, NULL, 0);
	return SteamUGC()->ReleaseQueryUGCRequest(handle);
}
DEFINE_PRIM(_BOOL, release_query_ugc_request, _BYTES);

HL_PRIM vbyte *HL_NAME(get_query_ugc_key_value_tag)(vbyte *cHandle, int iIndex, int iKeyValueTagIndex, int keySize, int valueSize){
	if (!CheckInit()) return (vbyte*)"";

	UGCQueryHandle_t handle = strtoull((char*)cHandle, NULL, 0);
	uint32 index = iIndex;
	uint32 keyValueTagIndex = iKeyValueTagIndex;
	uint32 cchKeySize = keySize;
	uint32 cchValueSize = valueSize;

	char *pchKey = new char[cchKeySize];
	char *pchValue = new char[cchValueSize];

	SteamUGC()->GetQueryUGCKeyValueTag(handle, index, keyValueTagIndex, pchKey, cchKeySize, pchValue, cchValueSize);

	std::ostringstream data;
	data << pchKey << "=" << pchValue;

	delete pchKey;
	delete pchValue;

	return (vbyte*)data.str().c_str();
}
DEFINE_PRIM(_BYTES, get_query_ugc_key_value_tag, _BYTES _I32 _I32 _I32 _I32);

HL_PRIM vbyte *HL_NAME(get_query_ugc_metadata)(vbyte *sHandle, int iIndex, int iMetaDataSize){
	if (!CheckInit()) return (vbyte*)("");

	UGCQueryHandle_t handle = strtoull((char*)sHandle, NULL, 0);

	uint32 cchMetadatasize = iMetaDataSize;
	char * pchMetadata = new char[cchMetadatasize];
	uint32 index = iIndex;

	SteamUGC()->GetQueryUGCMetadata(handle, index, pchMetadata, cchMetadatasize);

	std::ostringstream data;
	data << pchMetadata;

	delete pchMetadata;

	return (vbyte*)data.str().c_str();
}
DEFINE_PRIM(_BYTES, get_query_ugc_metadata, _BYTES _I32 _I32);

HL_PRIM vbyte *HL_NAME(get_query_ugc_result)(vbyte *sHandle, int iIndex){
	if (!CheckInit()) return (vbyte*)"";

	UGCQueryHandle_t handle = strtoull((char*)sHandle, NULL, 0);

	uint32 index = iIndex;

	SteamUGCDetails_t * d = new SteamUGCDetails_t;

	SteamUGC()->GetQueryUGCResult(handle, index, d);

	std::ostringstream data;

	data << "publishedFileID:" << d->m_nPublishedFileId << ",";
	data << "result:" << d->m_eResult << ",";
	data << "fileType:" << d->m_eFileType<< ",";
	data << "creatorAppID:" << d->m_nCreatorAppID<< ",";
	data << "consumerAppID:" << d->m_nConsumerAppID<< ",";
	data << "title:" << d->m_rgchTitle<< ",";
	data << "description:" << d->m_rgchDescription<< ",";
	data << "steamIDOwner:" << d->m_ulSteamIDOwner<< ",";
	data << "timeCreated:" << d->m_rtimeCreated<< ",";
	data << "timeUpdated:" << d->m_rtimeUpdated<< ",";
	data << "timeAddedToUserList:" << d->m_rtimeAddedToUserList<< ",";
	data << "visibility:" << d->m_eVisibility<< ",";
	data << "banned:" << d->m_bBanned<< ",";
	data << "acceptedForUse:" << d->m_bAcceptedForUse<< ",";
	data << "tagsTruncated:" << d->m_bTagsTruncated<< ",";
	data << "tags:" << d->m_rgchTags<< ",";
	data << "file:" << d->m_hFile<< ",";
	data << "previewFile:" << d->m_hPreviewFile<< ",";
	data << "fileName:" << d->m_pchFileName<< ",";
	data << "fileSize:" << d->m_nFileSize<< ",";
	data << "previewFileSize:" << d->m_nPreviewFileSize<< ",";
	data << "rgchURL:" << d->m_rgchURL<< ",";
	data << "votesup:" << d->m_unVotesUp<< ",";
	data << "votesDown:" << d->m_unVotesDown<< ",";
	data << "score:" << d->m_flScore<< ",";
	data << "numChildren:" << d->m_unNumChildren;

	delete d;

	return (vbyte*)data.str().c_str();
}
DEFINE_PRIM(_BYTES, get_query_ugc_result, _BYTES _I32);

HL_PRIM bool HL_NAME(submit_ugc_item_update)(vbyte *updateHandle, vbyte *changeNotes){
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	s_callbackHandler->SubmitUGCItemUpdate(updateHandle64, (char*)changeNotes);
 	return true;
}
DEFINE_PRIM(_BOOL, submit_ugc_item_update, _BYTES _BYTES);

HL_PRIM vbyte *HL_NAME(start_update_ugc_item)(int id, int itemID) {
	if (!CheckInit()) return (vbyte*)"0";

	UGCUpdateHandle_t ugcUpdateHandle = SteamUGC()->StartItemUpdate(id, itemID);

	//Change the uint64 to string, easier to handle between haxe & cpp.
	std::ostringstream updateHandleStream;
	updateHandleStream << ugcUpdateHandle;

 	return (vbyte*)updateHandleStream.str().c_str();
}
DEFINE_PRIM(_BYTES, start_update_ugc_item, _I32 _I32);

HL_PRIM bool HL_NAME(set_ugc_item_title)(vbyte *updateHandle, vbyte *title) {
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;
	return SteamUGC()->SetItemTitle(updateHandle64, (char*)title);
}
DEFINE_PRIM(_BOOL, set_ugc_item_title, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_ugc_item_description)(vbyte *updateHandle, vbyte *description){
	if(!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	return SteamUGC()->SetItemDescription(updateHandle64, (char*)description);
}
DEFINE_PRIM(_BOOL, set_ugc_item_description, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_ugc_item_tags)(vbyte *updateHandle, vbyte *tags){
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	// Create tag array from the string.
	SteamParamStringArray_t *pTags = getSteamParamStringArray((char*)tags);

	bool result = SteamUGC()->SetItemTags(updateHandle64, pTags);
	deleteSteamParamStringArray(pTags);
	return result;
}
DEFINE_PRIM(_BOOL, set_ugc_item_tags, _BYTES _BYTES);

HL_PRIM bool HL_NAME(add_ugc_item_key_value_tag)(vbyte *updateHandle, vbyte *keyStr, vbyte *valueStr){
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	return SteamUGC()->AddItemKeyValueTag(updateHandle64, (char*)keyStr, (char*)valueStr);
}
DEFINE_PRIM(_BOOL, add_ugc_item_key_value_tag, _BYTES _BYTES _BYTES);

HL_PRIM bool HL_NAME(remove_ugc_item_key_value_tags)(vbyte *updateHandle, vbyte *keyStr) {
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	return SteamUGC()->RemoveItemKeyValueTags(updateHandle64, (char*)keyStr);
}
DEFINE_PRIM(_BOOL, remove_ugc_item_key_value_tags, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_ugc_item_visibility)(vbyte *updateHandle, int visibility) {
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	ERemoteStoragePublishedFileVisibility visibilityEnum = static_cast<ERemoteStoragePublishedFileVisibility>(visibility);

	return SteamUGC()->SetItemVisibility(updateHandle64, visibilityEnum);
}
DEFINE_PRIM(_BOOL, set_ugc_item_visibility, _BYTES _I32);

HL_PRIM bool HL_NAME(set_ugc_item_content)(vbyte *updateHandle, vbyte *path){
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	return SteamUGC()->SetItemContent(updateHandle64, (char*)path);
}
DEFINE_PRIM(_BOOL, set_ugc_item_content, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_ugc_item_preview_image)(vbyte *updateHandle, vbyte *path){
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	return SteamUGC()->SetItemPreview(updateHandle64, (char*)path);
}
DEFINE_PRIM(_BOOL, set_ugc_item_preview_image, _BYTES _BYTES);

HL_PRIM bool HL_NAME(create_ugc_item)(int id) {
	if (!CheckInit()) return false;

	s_callbackHandler->CreateUGCItem(id, k_EWorkshopFileTypeCommunity);
 	return true;
}
DEFINE_PRIM(_BOOL, create_ugc_item, _I32);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM bool HL_NAME(add_required_tag)(vbyte* handle, vbyte *tagName){
	if (!CheckInit()) return false;
	UGCQueryHandle_t u64Handle = strtoull((char*)handle, NULL, 0);
	return SteamUGC()->AddRequiredTag(u64Handle, (char*)tagName);
}
DEFINE_PRIM(_BOOL, add_required_tag, _BYTES _BYTES);

HL_PRIM bool HL_NAME(add_required_key_value_tag)(vbyte *handle, vbyte *pKey, vbyte *pValue){
	if (!CheckInit()) return false;
	UGCQueryHandle_t u64Handle = strtoull((char*)handle, NULL, 0);
	return SteamUGC()->AddRequiredKeyValueTag(u64Handle, (char*)pKey, (char*)pValue);
}
DEFINE_PRIM(_BOOL, add_required_key_value_tag, _BYTES _BYTES _BYTES);

HL_PRIM bool HL_NAME(add_excluded_tag)(vbyte *handle, vbyte *tagName){
	if (!CheckInit()) return false;
	UGCQueryHandle_t u64Handle = strtoull((char*)handle, NULL, 0);
	return SteamUGC()->AddExcludedTag(u64Handle, (char*)tagName);
}
DEFINE_PRIM(_BOOL, add_excluded_tag, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_return_metadata)(vbyte *handle, bool returnMetadata){
	if (!CheckInit()) return false;
	UGCQueryHandle_t u64Handle = strtoull((char*)handle, NULL, 0);
	return SteamUGC()->SetReturnMetadata(u64Handle, returnMetadata);
}
DEFINE_PRIM(_BOOL, set_return_metadata, _BYTES _BOOL);

HL_PRIM bool HL_NAME(set_return_key_value_tags)(vbyte *handle, bool returnKeyValueTags){
	if (!CheckInit()) return false;
	UGCQueryHandle_t u64Handle = strtoull((char*)handle, NULL, 0);
	return SteamUGC()->SetReturnKeyValueTags(u64Handle, returnKeyValueTags);
}
DEFINE_PRIM(_BOOL, set_return_key_value_tags, _BYTES _BOOL);
