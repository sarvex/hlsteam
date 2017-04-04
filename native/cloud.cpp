#include "steamwrap.h"

void CallbackHandler::FileShare(const char * fileName){
	SteamAPICall_t hSteamAPICall = SteamRemoteStorage()->FileShare(fileName);
	m_callResultFileShare.Set(hSteamAPICall, this, &CallbackHandler::OnFileShared);
}

void CallbackHandler::OnFileShared(RemoteStorageFileShareResult_t *pCallback, bool bIOFailure){
	if (pCallback->m_eResult == k_EResultOK && !bIOFailure)	{
		UGCHandle_t rawHandle = pCallback->m_hFile;

		//convert uint64 handle to string
		std::ostringstream strHandle;
		strHandle << rawHandle;

		SendEvent(RemoteStorageFileShared, true, strHandle.str().c_str());
	}else{
		SendEvent(RemoteStorageFileShared, false, NULL);
	}
}

HL_PRIM int HL_NAME(get_file_count)(){
	int fileCount = SteamRemoteStorage()->GetFileCount();
	return fileCount;
}
DEFINE_PRIM(_I32, get_file_count, _NO_ARG);

HL_PRIM int HL_NAME(get_file_size)(vbyte *fileName){
	int fileSize = SteamRemoteStorage()->GetFileSize((char*)fileName);
	return fileSize;
}
DEFINE_PRIM(_I32, get_file_size, _BYTES);

HL_PRIM bool HL_NAME(file_exists)(vbyte *fileName){
	return SteamRemoteStorage()->FileExists((char*)fileName);
}
DEFINE_PRIM(_BOOL, file_exists, _BYTES);

HL_PRIM vbyte *HL_NAME(file_read)(vbyte *fileName, int *len){
	if (!CheckInit()) return NULL;

	const char * fName = (char*)fileName;

	bool exists = SteamRemoteStorage()->FileExists(fName);
	if(!exists) return NULL;

	int length = SteamRemoteStorage()->GetFileSize(fName);

	char *bytesData = (char *)hl_gc_alloc_noptr(length);
	int32 result = SteamRemoteStorage()->FileRead(fName, bytesData, length);

	*len = length;
	return (vbyte*)bytesData;
}
DEFINE_PRIM(_BYTES, file_read, _BYTES _REF(_I32));

HL_PRIM bool HL_NAME(file_write)(vbyte *fileName, vbyte *bytes, int length){
	if (!CheckInit()) return false;
	if (length <= 0) return false;
	return SteamRemoteStorage()->FileWrite((char*)fileName, (char*)bytes, length);
}
DEFINE_PRIM(_BOOL, file_write, _BYTES _BYTES _I32);

HL_PRIM bool HL_NAME(file_delete)(vbyte *fileName){
	return SteamRemoteStorage()->FileDelete((char*)fileName);
}
DEFINE_PRIM(_BOOL, file_delete, _BYTES);

HL_PRIM void HL_NAME(file_share)(vbyte *fileName){
	if( !CheckInit() ) return;
	s_callbackHandler->FileShare((char*)fileName);
}
DEFINE_PRIM(_VOID, file_share, _BYTES);

HL_PRIM bool HL_NAME(is_cloud_enabled_for_app)(){
	return SteamRemoteStorage()->IsCloudEnabledForApp();
}
DEFINE_PRIM(_BOOL, is_cloud_enabled_for_app, _NO_ARG);

HL_PRIM void HL_NAME(set_cloud_enabled_for_app)(bool enabled){
	SteamRemoteStorage()->SetCloudEnabledForApp(enabled);
}
DEFINE_PRIM(_VOID, set_cloud_enabled_for_app, _BOOL);

HL_PRIM void HL_NAME(get_quota)( double *total, double *available ){
	uint64 iTotal = 0;
	uint64 iAvailable = 0;

	SteamRemoteStorage()->GetQuota(&iTotal, &iAvailable);

	*total = (double)iTotal;
	*available = (double)iAvailable;
}
DEFINE_PRIM(_VOID, get_quota, _NO_ARG _REF(_F64) _REF(_F64));