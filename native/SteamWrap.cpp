#define HL_NAME(n) steam_##n
#include <hl.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <map>

#include <steam/steam_api.h>

//just splits a string
void split(const std::string &s, char delim, std::vector<std::string> &elems) {
	std::stringstream ss;
	ss.str(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
}

//generates a parameter string array from comma-delimeted-values
SteamParamStringArray_t * getSteamParamStringArray(const char * str){
	std::string stdStr = str;
	
	//NOTE: this will probably fail if the string includes Unicode, but Steam tags probably don't support that?
	std::vector<std::string> v;
	split(stdStr, ',', v);
	
	SteamParamStringArray_t * params = new SteamParamStringArray_t;
	
	int count = v.size();
	
	params->m_nNumStrings = (int32) count;
	params->m_ppStrings = new const char *[count];
	
	for(int i = 0; i < count; i++) {
		params->m_ppStrings[i] = v[i].c_str();
	}
	
	return params;
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

void deleteSteamParamStringArray(SteamParamStringArray_t * params){
	for(int i = 0; i < params->m_nNumStrings; i++){
		delete params->m_ppStrings[i];
	}
	delete[] params->m_ppStrings;
	delete params;
}


//-----------------------------------------------------------------------------------------------------------
// Event
//-----------------------------------------------------------------------------------------------------------

vclosure *g_eventHandler = 0;

typedef enum {
	None,
	GamepadTextInputDismissed,
	UserStatsReceived,
	UserStatsStored,
	UserAchievementStored,
	LeaderboardFound,
	ScoreUploaded,
	ScoreDownloaded,
	GlobalStatsReceived,
	UGCLegalAgreementStatus,
	UGCItemCreated,
	UGCItemUpdateSubmitted,
	RemoteStorageFileShared,
	UserSharedWorkshopFilesEnumerated,
	UserPublishedFilesEnumerated,
	UserSubscribedFilesEnumerated,
	UGCDownloaded,
	PublishedFileDetailsGotten,
	ItemDownloaded,
	ItemInstalled,
	UGCQueryCompleted
} event_type;

//A simple data structure that holds on to the native 64-bit handles and maps them to regular ints.
//This is because it is cumbersome to pass back 64-bit values over CFFI, and strictly speaking, the haxe 
//side never needs to know the actual values. So we just store the full 64-bit values locally and pass back 
//0-based index values which easily fit into a regular int.
class steamHandleMap{
	//TODO: figure out templating or whatever so I can make typed versions of this like in Haxe (steamHandleMap<ControllerHandle_t>)
	//      all the steam handle typedefs are just renamed uint64's, but this could always change so to be 100% super safe I should
	//      figure out the templating stuff.
	
	private:
		std::map<int, uint64> values;
		std::map<int, uint64>::iterator it;
		int maxKey;
		
	public:
		
		void init()		{
			values.clear();
			maxKey = -1;
		}
		
		bool exists(uint64 val){
			return find(val) >= 0;
		}
		
		int find(uint64 val){
			for(int i = 0; i <= maxKey; i++){
				if(values[i] == val)
					return i;
			}
			return -1;
		}
		
		uint64 get(int index){
			return values[index];
		}
		
		//add a unique uint64 value to this data structure & return what index it was stored at
		int add(uint64 val)	{
			int i = find(val);
			
			//if it already exists just return where it is stored
			if(i >= 0)
				return i;
			
			//if it is unique increase our maxKey count and return that
			maxKey++;
			values[maxKey] = val;
			
			return maxKey;
		}
};

static steamHandleMap mapControllers;
static ControllerAnalogActionData_t analogActionData;
static ControllerMotionData_t motionData;

static void SendEvent(event_type type, bool success, const char *data) {
	if (!g_eventHandler) return;
	if (g_eventHandler->hasValue)
		((void(*)(void*, event_type, bool, vbyte*))g_eventHandler->fun)(g_eventHandler->value, type, success, (vbyte*)data);
	else
		((void(*)(event_type, bool, vbyte*))g_eventHandler->fun)(type, success, (vbyte*)data);
}


//-----------------------------------------------------------------------------------------------------------
// CallbackHandler
//-----------------------------------------------------------------------------------------------------------
class CallbackHandler {
private:
	//SteamLeaderboard_t m_curLeaderboard;
	std::map<std::string, SteamLeaderboard_t> m_leaderboards;
public:

	CallbackHandler() :
 		m_CallbackUserStatsReceived( this, &CallbackHandler::OnUserStatsReceived ),
 		m_CallbackUserStatsStored( this, &CallbackHandler::OnUserStatsStored ),
 		m_CallbackAchievementStored( this, &CallbackHandler::OnAchievementStored ),
		m_CallbackGamepadTextInputDismissed( this, &CallbackHandler::OnGamepadTextInputDismissed ),
		m_CallbackDownloadItemResult( this, &CallbackHandler::OnDownloadItem ),
		m_CallbackItemInstalled( this, &CallbackHandler::OnItemInstalled )
	{}

	STEAM_CALLBACK( CallbackHandler, OnUserStatsReceived, UserStatsReceived_t, m_CallbackUserStatsReceived );
	STEAM_CALLBACK( CallbackHandler, OnUserStatsStored, UserStatsStored_t, m_CallbackUserStatsStored );
	STEAM_CALLBACK( CallbackHandler, OnAchievementStored, UserAchievementStored_t, m_CallbackAchievementStored );
	STEAM_CALLBACK( CallbackHandler, OnGamepadTextInputDismissed, GamepadTextInputDismissed_t, m_CallbackGamepadTextInputDismissed );
	STEAM_CALLBACK( CallbackHandler, OnDownloadItem, DownloadItemResult_t, m_CallbackDownloadItemResult );
	STEAM_CALLBACK( CallbackHandler, OnItemInstalled, ItemInstalled_t, m_CallbackItemInstalled );
	
	void FindLeaderboard(const char* name);
	void OnLeaderboardFound( LeaderboardFindResult_t *pResult, bool bIOFailure);
	CCallResult<CallbackHandler, LeaderboardFindResult_t> m_callResultFindLeaderboard;

	bool UploadScore(const std::string& leaderboardId, int score, int detail);
	void OnScoreUploaded( LeaderboardScoreUploaded_t *pResult, bool bIOFailure);
	CCallResult<CallbackHandler, LeaderboardScoreUploaded_t> m_callResultUploadScore;

	bool DownloadScores(const std::string& leaderboardId, int numBefore, int numAfter);
	void OnScoreDownloaded( LeaderboardScoresDownloaded_t *pResult, bool bIOFailure);
	CCallResult<CallbackHandler, LeaderboardScoresDownloaded_t> m_callResultDownloadScore;

	void RequestGlobalStats();
	void OnGlobalStatsReceived(GlobalStatsReceived_t* pResult, bool bIOFailure);
	CCallResult<CallbackHandler, GlobalStatsReceived_t> m_callResultRequestGlobalStats;

	void CreateUGCItem(AppId_t nConsumerAppId, EWorkshopFileType eFileType);
	void OnUGCItemCreated( CreateItemResult_t *pResult, bool bIOFailure);
	CCallResult<CallbackHandler, CreateItemResult_t> m_callResultCreateUGCItem;
	
	void SendQueryUGCRequest(UGCQueryHandle_t handle);
	void OnUGCQueryCompleted( SteamUGCQueryCompleted_t* pResult, bool bIOFailure); 
	CCallResult<CallbackHandler, SteamUGCQueryCompleted_t> m_callResultUGCQueryCompleted;
	
	void SubmitUGCItemUpdate(UGCUpdateHandle_t handle, const char *pchChangeNote);
	void OnItemUpdateSubmitted( SubmitItemUpdateResult_t *pResult, bool bIOFailure);
	CCallResult<CallbackHandler, SubmitItemUpdateResult_t> m_callResultSubmitUGCItemUpdate;
	
	void EnumerateUserSharedWorkshopFiles( CSteamID steamId, uint32 unStartIndex, SteamParamStringArray_t *pRequiredTags, SteamParamStringArray_t *pExcludedTags );
	void OnEnumerateUserSharedWorkshopFiles( RemoteStorageEnumerateUserPublishedFilesResult_t * pResult, bool bIOFailure);
	CCallResult<CallbackHandler, RemoteStorageEnumerateUserPublishedFilesResult_t > m_callResultEnumerateUserSharedWorkshopFiles;
	
	void EnumerateUserSubscribedFiles ( uint32 unStartIndex );
	void OnEnumerateUserSubscribedFiles ( RemoteStorageEnumerateUserSubscribedFilesResult_t * pResult, bool bIOFailure);
	CCallResult<CallbackHandler, RemoteStorageEnumerateUserSubscribedFilesResult_t > m_callResultEnumerateUserSubscribedFiles;
	
	void EnumerateUserPublishedFiles ( uint32 unStartIndex );
	void OnEnumerateUserPublishedFiles ( RemoteStorageEnumerateUserPublishedFilesResult_t * pResult, bool bIOFailure);
	CCallResult<CallbackHandler, RemoteStorageEnumerateUserPublishedFilesResult_t > m_callResultEnumerateUserPublishedFiles;
	
	void UGCDownload ( UGCHandle_t hContent, uint32 unPriority );
	void OnUGCDownload ( RemoteStorageDownloadUGCResult_t * pResult, bool bIOFailure);
	CCallResult<CallbackHandler, RemoteStorageDownloadUGCResult_t  > m_callResultUGCDownload;
	
	void GetPublishedFileDetails ( PublishedFileId_t unPublishedFileId, uint32 unMaxSecondsOld );
	void OnGetPublishedFileDetails ( RemoteStorageGetPublishedFileDetailsResult_t * pResult, bool bIOFailure);
	CCallResult<CallbackHandler, RemoteStorageGetPublishedFileDetailsResult_t > m_callResultGetPublishedFileDetails;
	
	void FileShare(const char* fileName);
	void OnFileShared( RemoteStorageFileShareResult_t *pResult, bool bIOFailure);
	CCallResult<CallbackHandler, RemoteStorageFileShareResult_t> m_callResultFileShare;
	
};

void CallbackHandler::OnGamepadTextInputDismissed( GamepadTextInputDismissed_t *pCallback ){
	SendEvent(GamepadTextInputDismissed, pCallback->m_bSubmitted, NULL);
}

void CallbackHandler::OnUserStatsReceived( UserStatsReceived_t *pCallback ){
 	if (pCallback->m_nGameID != SteamUtils()->GetAppID()) return;
	SendEvent(UserStatsReceived, pCallback->m_eResult == k_EResultOK, NULL);
}

void CallbackHandler::OnUserStatsStored( UserStatsStored_t *pCallback ){
 	if (pCallback->m_nGameID != SteamUtils()->GetAppID()) return;
	SendEvent(UserStatsStored, pCallback->m_eResult == k_EResultOK, NULL);
}

void CallbackHandler::OnAchievementStored( UserAchievementStored_t *pCallback ){
 	if (pCallback->m_nGameID != SteamUtils()->GetAppID()) return;
	SendEvent(UserAchievementStored, true, pCallback->m_rgchAchievementName);
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

void CallbackHandler::FindLeaderboard(const char* name){
	m_leaderboards[name] = 0;
 	SteamAPICall_t hSteamAPICall = SteamUserStats()->FindLeaderboard(name);
 	m_callResultFindLeaderboard.Set(hSteamAPICall, this, &CallbackHandler::OnLeaderboardFound);
}

void CallbackHandler::OnLeaderboardFound(LeaderboardFindResult_t *pCallback, bool bIOFailure)
{
	// see if we encountered an error during the call
	if (pCallback->m_bLeaderboardFound && !bIOFailure)	{
		std::string leaderboardId = SteamUserStats()->GetLeaderboardName(pCallback->m_hSteamLeaderboard);
		m_leaderboards[leaderboardId] = pCallback->m_hSteamLeaderboard;
		SendEvent(LeaderboardFound, true, leaderboardId.c_str());
	}else{
		SendEvent(LeaderboardFound, false, NULL);
	}
}

bool CallbackHandler::UploadScore(const std::string& leaderboardId, int score, int detail){
   	if (m_leaderboards.find(leaderboardId) == m_leaderboards.end() || m_leaderboards[leaderboardId] == 0)
   		return false;

	SteamAPICall_t hSteamAPICall = SteamUserStats()->UploadLeaderboardScore(m_leaderboards[leaderboardId], k_ELeaderboardUploadScoreMethodKeepBest, score, &detail, 1);
	m_callResultUploadScore.Set(hSteamAPICall, this, &CallbackHandler::OnScoreUploaded);
 	return true;
}

void CallbackHandler::FileShare(const char * fileName){
	SteamAPICall_t hSteamAPICall = SteamRemoteStorage()->FileShare(fileName);
	m_callResultFileShare.Set(hSteamAPICall, this, &CallbackHandler::OnFileShared);
}

static std::string toLeaderboardScore(const char* leaderboardName, int score, int detail, int rank){
	std::ostringstream data;
	data << leaderboardName << "," << score << "," << detail << "," << rank;
	return data.str();
}

void CallbackHandler::OnScoreUploaded(LeaderboardScoreUploaded_t *pCallback, bool bIOFailure){
	if (pCallback->m_bSuccess && !bIOFailure){
		std::string leaderboardName = SteamUserStats()->GetLeaderboardName(pCallback->m_hSteamLeaderboard);
		std::string data = toLeaderboardScore(SteamUserStats()->GetLeaderboardName(pCallback->m_hSteamLeaderboard), pCallback->m_nScore, -1, pCallback->m_nGlobalRankNew);
		SendEvent(ScoreUploaded, true, data.c_str());
	}else if (pCallback != NULL && pCallback->m_hSteamLeaderboard != 0) {
		SendEvent(ScoreUploaded, false, SteamUserStats()->GetLeaderboardName(pCallback->m_hSteamLeaderboard));
	}else{
		SendEvent(ScoreUploaded, false, NULL);
	}
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

bool CallbackHandler::DownloadScores(const std::string& leaderboardId, int numBefore, int numAfter){
   	if (m_leaderboards.find(leaderboardId) == m_leaderboards.end() || m_leaderboards[leaderboardId] == 0)
   		return false;

 	// load the specified leaderboard data around the current user
 	SteamAPICall_t hSteamAPICall = SteamUserStats()->DownloadLeaderboardEntries(m_leaderboards[leaderboardId], k_ELeaderboardDataRequestGlobalAroundUser, -numBefore, numAfter);
	m_callResultDownloadScore.Set(hSteamAPICall, this, &CallbackHandler::OnScoreDownloaded);

 	return true;
}

void CallbackHandler::OnScoreDownloaded(LeaderboardScoresDownloaded_t *pCallback, bool bIOFailure){
	if (bIOFailure)	{
		SendEvent(ScoreDownloaded, false, NULL);
		return;
	}

	std::string leaderboardId = SteamUserStats()->GetLeaderboardName(pCallback->m_hSteamLeaderboard);

	int numEntries = pCallback->m_cEntryCount;
	if (numEntries > 10) numEntries = 10;

	std::ostringstream data;
	bool haveData = false;

	for (int i=0; i<numEntries; i++){
		int score = 0;
		int details[1];
		LeaderboardEntry_t entry;
		SteamUserStats()->GetDownloadedLeaderboardEntry(pCallback->m_hSteamLeaderboardEntries, i, &entry, details, 1);
		if (entry.m_cDetails != 1) continue;

		if (haveData) 
			data << ";";
		data << toLeaderboardScore(leaderboardId.c_str(), entry.m_nScore, details[0], entry.m_nGlobalRank).c_str();
		haveData = true;
	}

	if (haveData)
		SendEvent(ScoreDownloaded, true, data.str().c_str());
	else
		SendEvent(ScoreDownloaded, true, toLeaderboardScore(leaderboardId.c_str(), -1, -1, -1).c_str());
}

void CallbackHandler::RequestGlobalStats(){
 	SteamAPICall_t hSteamAPICall = SteamUserStats()->RequestGlobalStats(0);
 	m_callResultRequestGlobalStats.Set(hSteamAPICall, this, &CallbackHandler::OnGlobalStatsReceived);
}

void CallbackHandler::OnGlobalStatsReceived(GlobalStatsReceived_t* pResult, bool bIOFailure){
	if (!bIOFailure){
		if (pResult->m_nGameID != SteamUtils()->GetAppID()) return;
		SendEvent(GlobalStatsReceived, pResult->m_eResult == k_EResultOK, NULL);
	}else{
		SendEvent(GlobalStatsReceived, false, NULL);
	}
}

void CallbackHandler::EnumerateUserPublishedFiles( uint32 unStartIndex ){
	SteamAPICall_t hSteamAPICall = SteamRemoteStorage()->EnumerateUserPublishedFiles(unStartIndex);
	m_callResultEnumerateUserPublishedFiles.Set(hSteamAPICall, this, &CallbackHandler::OnEnumerateUserPublishedFiles);
}

void CallbackHandler::OnEnumerateUserPublishedFiles(RemoteStorageEnumerateUserPublishedFilesResult_t* pResult, bool bIOFailure){
	if (!bIOFailure){
		if(pResult->m_eResult == k_EResultOK){
			std::ostringstream data;
			
			data << "result:";
			data << pResult->m_eResult;
			data << ",resultsReturned:";
			data << pResult->m_nResultsReturned;
			data << ",totalResults:";
			data << pResult->m_nTotalResultCount;
			data << ",publishedFileIds:";
			
			for(int32 i = 0; i < pResult->m_nResultsReturned; ++i) {
				data << pResult->m_rgPublishedFileId[i];
				if(i != pResult->m_nResultsReturned-1)
					data << ',';
			}
			
			SendEvent(UserPublishedFilesEnumerated, pResult->m_eResult == k_EResultOK, data.str().c_str());
			return;
		}
	}
	SendEvent(UserSharedWorkshopFilesEnumerated, false, NULL);
}

void CallbackHandler::EnumerateUserSharedWorkshopFiles( CSteamID steamId, uint32 unStartIndex, SteamParamStringArray_t *pRequiredTags, SteamParamStringArray_t *pExcludedTags ){
	SteamAPICall_t hSteamAPICall = SteamRemoteStorage()->EnumerateUserSharedWorkshopFiles(steamId, unStartIndex, pRequiredTags, pExcludedTags);
	m_callResultEnumerateUserSharedWorkshopFiles.Set(hSteamAPICall, this, &CallbackHandler::OnEnumerateUserSharedWorkshopFiles);
}

void CallbackHandler::OnEnumerateUserSharedWorkshopFiles(RemoteStorageEnumerateUserPublishedFilesResult_t* pResult, bool bIOFailure){
	if(pResult->m_eResult == k_EResultOK)	{
		std::ostringstream data;
		
		data << "result:";
		data << pResult->m_eResult;
		data << ",resultsReturned:";
		data << pResult->m_nResultsReturned;
		data << ",totalResults:";
		data << pResult->m_nTotalResultCount;
		data << ",publishedFileIds:";
		
		for(int32 i = 0; i < pResult->m_nResultsReturned; ++i) {
			data << pResult->m_rgPublishedFileId[i];
			if(i != pResult->m_nResultsReturned-1)
				data << ',';
		}
		
		SendEvent(UserSharedWorkshopFilesEnumerated, pResult->m_eResult == k_EResultOK, data.str().c_str());
		return;
	}
	SendEvent(UserSharedWorkshopFilesEnumerated, false, NULL);
}

void CallbackHandler::EnumerateUserSubscribedFiles( uint32 unStartIndex ){
	SteamAPICall_t hSteamAPICall = SteamRemoteStorage()->EnumerateUserSubscribedFiles( unStartIndex );
	m_callResultEnumerateUserSubscribedFiles.Set(hSteamAPICall, this, &CallbackHandler::OnEnumerateUserSubscribedFiles);
}

void CallbackHandler::OnEnumerateUserSubscribedFiles(RemoteStorageEnumerateUserSubscribedFilesResult_t* pResult, bool bIOFailure){
	if(pResult->m_eResult == k_EResultOK)	{
		std::ostringstream data;
		
		data << "result:";
		data << pResult->m_eResult;
		data << ",resultsReturned:";
		data << pResult->m_nResultsReturned;
		data << ",totalResults:";
		data << pResult->m_nTotalResultCount;
		data << ",publishedFileIds:";
		
		for(int32 i = 0; i < pResult->m_nResultsReturned; ++i) {
			
			data << pResult->m_rgPublishedFileId[i];
			if(i != pResult->m_nResultsReturned-1){
				data << ',';
			}
			
		}
		
		data << ",timeSubscribed:";
		
		for(int32 i = 0; i < pResult->m_nResultsReturned; ++i) {
			data << pResult->m_rgRTimeSubscribed[i];
			if(i != pResult->m_nResultsReturned-1)
				data << ',';
		}
		
		SendEvent(UserSubscribedFilesEnumerated, pResult->m_eResult == k_EResultOK, data.str().c_str());
		return;
	}
	SendEvent(UserSubscribedFilesEnumerated, false, NULL);
}

void CallbackHandler::GetPublishedFileDetails( PublishedFileId_t unPublishedFileId, uint32 unMaxSecondsOld ){
	SteamAPICall_t hSteamAPICall = SteamRemoteStorage()->GetPublishedFileDetails( unPublishedFileId, unMaxSecondsOld);
	m_callResultGetPublishedFileDetails.Set(hSteamAPICall, this, &CallbackHandler::OnGetPublishedFileDetails);
}

void CallbackHandler::OnGetPublishedFileDetails(RemoteStorageGetPublishedFileDetailsResult_t* pResult, bool bIOFailure){
	if(pResult->m_eResult == k_EResultOK)	{
		std::ostringstream data;
		
		data << "result:";
		data << pResult->m_eResult;
		data << ",publishedFileID:";
		data << pResult->m_nPublishedFileId;
		data << ",creatorAppID:";
		data << pResult->m_nCreatorAppID;
		data << ",consumerAppID:";
		data << pResult->m_nConsumerAppID;
		data << ",title:";
		data << pResult->m_rgchTitle;
		data << ",description:";
		data << pResult->m_rgchDescription;
		data << ",fileHandle:";
		data << pResult->m_hFile;
		data << ",previewFileHandle:";
		data << pResult->m_hPreviewFile;
		data << ",steamIDOwner:";
		data << pResult->m_ulSteamIDOwner;
		data << ",timeCreated:";
		data << pResult->m_rtimeCreated;
		data << ",timeUpdated";
		data << pResult->m_rtimeUpdated;
		data << ",visibility:";
		data << pResult->m_eVisibility;
		data << ",banned:";
		data << pResult->m_bBanned;
		data << ",tags:";
		data << pResult->m_rgchTags;
		data << ",tagsTruncated:";
		data << pResult->m_bTagsTruncated;
		data << ",fileName:";
		data << pResult->m_pchFileName;
		data << ",fileSize:";
		data << pResult->m_nFileSize;
		data << ",previewFileSize:";
		data << pResult->m_nPreviewFileSize;
		data << ",url:";
		data << pResult->m_rgchURL;
		data << ",fileType:",
		data << pResult->m_eFileType;
		data << ",acceptedForUse:",
		data << pResult->m_bAcceptedForUse;
		
		SendEvent(PublishedFileDetailsGotten, pResult->m_eResult == k_EResultOK, data.str().c_str());
		return;
	}
	SendEvent(PublishedFileDetailsGotten, false, NULL);
}

void CallbackHandler::UGCDownload( UGCHandle_t hContent, uint32 unPriority ){
	SteamAPICall_t hSteamAPICall = SteamRemoteStorage()->UGCDownload( hContent, unPriority );
	m_callResultUGCDownload.Set(hSteamAPICall, this, &CallbackHandler::OnUGCDownload);
}

void CallbackHandler::OnUGCDownload(RemoteStorageDownloadUGCResult_t* pResult, bool bIOFailure){
	if(pResult->m_eResult == k_EResultOK){
		std::ostringstream data;
		
		data << "result:";
		data << pResult->m_eResult;
		data << ",fileHandle:";
		data << pResult->m_hFile;
		data << ",appID:";
		data << pResult->m_nAppID;
		data << ",sizeInBytes:";
		data << pResult->m_nSizeInBytes;
		data << ",fileName:";
		data << pResult->m_pchFileName;
		data << ",steamIDOwner:";
		data << pResult->m_ulSteamIDOwner;
		
		SendEvent(UGCDownloaded, pResult->m_eResult == k_EResultOK, data.str().c_str());
		return;
	}
	SendEvent(UGCDownloaded, false, NULL);
}

void CallbackHandler::OnDownloadItem( DownloadItemResult_t *pCallback ){
	if (pCallback->m_unAppID != SteamUtils()->GetAppID()) return;
	
	std::ostringstream fileIDStream;
	PublishedFileId_t m_ugcFileID = pCallback->m_nPublishedFileId;
	fileIDStream << m_ugcFileID;
	SendEvent(ItemDownloaded, pCallback->m_eResult == k_EResultOK, fileIDStream.str().c_str());
}

void CallbackHandler::OnItemInstalled( ItemInstalled_t *pCallback ){
	if (pCallback->m_unAppID != SteamUtils()->GetAppID()) return;
	
	std::ostringstream fileIDStream;
	PublishedFileId_t m_ugcFileID = pCallback->m_nPublishedFileId;
	fileIDStream << m_ugcFileID;
	SendEvent(ItemDownloaded, true, fileIDStream.str().c_str());
}

//-----------------------------------------------------------------------------------------------------------
static CallbackHandler* s_callbackHandler = NULL;

extern "C"
{

static bool CheckInit(){
	return SteamUser() && SteamUser()->BLoggedOn() && SteamUserStats() && (s_callbackHandler != 0) && (g_eventHandler != 0);
}

HL_PRIM bool HL_NAME(init)(vclosure *onEvent, int notificationPosition){
	bool result = SteamAPI_Init();
	if (result)	{
		g_eventHandler = onEvent;
		s_callbackHandler = new CallbackHandler();

		switch (notificationPosition){
			case 0:
				SteamUtils()->SetOverlayNotificationPosition(k_EPositionTopLeft);
				break;
			case 1:
				SteamUtils()->SetOverlayNotificationPosition(k_EPositionTopRight);
				break;
			case 2:
				SteamUtils()->SetOverlayNotificationPosition(k_EPositionBottomRight);
				break;
			case 3:
				SteamUtils()->SetOverlayNotificationPosition(k_EPositionBottomLeft);
				break;
			default:
				SteamUtils()->SetOverlayNotificationPosition(k_EPositionBottomRight);
				break;
		}
	}
	return result;
}
DEFINE_PRIM(_BOOL, init, _FUN(_VOID, _I32 _BOOL _BYTES) _I32);

HL_PRIM void HL_NAME(shutdown)(){
	SteamAPI_Shutdown();
	delete g_eventHandler;
	g_eventHandler = NULL;
	delete s_callbackHandler;
	s_callbackHandler = NULL;
}
DEFINE_PRIM(_VOID, shutdown, _NO_ARG);

HL_PRIM void HL_NAME(run_callbacks)(){
	SteamAPI_RunCallbacks();
}
DEFINE_PRIM(_VOID, run_callbacks, _NO_ARG);

HL_PRIM bool HL_NAME(open_overlay)(vbyte *url){
	if (!CheckInit()) return false;

	SteamFriends()->ActivateGameOverlayToWebPage((char*)url);
	return true;
}
DEFINE_PRIM(_BOOL, open_overlay, _BYTES);

//-----------------------------------------------------------------------------------------------------------

HL_PRIM bool HL_NAME(request_stats)(){
	if (!CheckInit()) return false;

	return SteamUserStats()->RequestCurrentStats();
}
DEFINE_PRIM(_BOOL, request_stats, _NO_ARG);

HL_PRIM int HL_NAME(get_stat_int)(vbyte *name){
	if (!CheckInit()) return 0;

	int val = 0;
	SteamUserStats()->GetStat((char*)name, &val);
	return val;
}
DEFINE_PRIM(_I32, get_stat_int, _BYTES);

HL_PRIM double HL_NAME(get_stat_float)(vbyte *name){
	if (!CheckInit()) return 0.0;

	float val = 0.0;
	SteamUserStats()->GetStat((char*)name, &val);
	return (double)val;
}
DEFINE_PRIM(_F64, get_stat_float, _BYTES);

HL_PRIM bool HL_NAME(set_stat_int)(vbyte *name, int val){
	if (!CheckInit()) return false;
	return SteamUserStats()->SetStat((char*)name, val);
}
DEFINE_PRIM(_BOOL, set_stat_int, _BYTES _I32);

HL_PRIM bool HL_NAME(set_stat_float)(vbyte *name, double val){
	if (!CheckInit()) return false;
	return SteamUserStats()->SetStat((char*)name, (float)val);
}
DEFINE_PRIM(_BOOL, set_stat_float, _BYTES _F64);

HL_PRIM bool HL_NAME(store_stats)(){
	if (!CheckInit()) return false;
	return SteamUserStats()->StoreStats();
}
DEFINE_PRIM(_BOOL, store_stats, _NO_ARG);

//-----------------------------------------------------------------------------------------------------------

HL_PRIM bool HL_NAME(submit_ugcitem_update)(vbyte *updateHandle, vbyte *changeNotes){
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	s_callbackHandler->SubmitUGCItemUpdate(updateHandle64, (char*)changeNotes);
 	return true;
}
DEFINE_PRIM(_BOOL, submit_ugcitem_update, _BYTES _BYTES);

HL_PRIM vbyte *HL_NAME(start_update_ugcitem)(int id, int itemID) {
	if (!CheckInit()) return (vbyte*)"0";

	UGCUpdateHandle_t ugcUpdateHandle = SteamUGC()->StartItemUpdate(id, itemID);

	//Change the uint64 to string, easier to handle between haxe & cpp.
	std::ostringstream updateHandleStream;
	updateHandleStream << ugcUpdateHandle;

 	return (vbyte*)updateHandleStream.str().c_str();
}
DEFINE_PRIM(_BYTES, start_update_ugcitem, _I32 _I32);

HL_PRIM bool HL_NAME(set_ugcitem_title)(vbyte *updateHandle, vbyte *title) {
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;
	return SteamUGC()->SetItemTitle(updateHandle64, (char*)title);
}
DEFINE_PRIM(_BOOL, set_ugcitem_title, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_ugcitem_description)(vbyte *updateHandle, vbyte *description){
	if(!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	return SteamUGC()->SetItemDescription(updateHandle64, (char*)description);
}
DEFINE_PRIM(_BOOL, set_ugcitem_description, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_ugcitem_tags)(vbyte *updateHandle, vbyte *tags){
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
DEFINE_PRIM(_BOOL, set_ugcitem_tags, _BYTES _BYTES);

HL_PRIM bool HL_NAME(add_ugcitem_key_value_tag)(vbyte *updateHandle, vbyte *keyStr, vbyte *valueStr){
	if (!CheckInit()) return false;
	
	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;
	
	return SteamUGC()->AddItemKeyValueTag(updateHandle64, (char*)keyStr, (char*)valueStr);
}
DEFINE_PRIM(_BOOL, add_ugcitem_key_value_tag, _BYTES _BYTES _BYTES);

HL_PRIM bool HL_NAME(remove_ugcitem_key_value_tags)(vbyte *updateHandle, vbyte *keyStr) {
	if (!CheckInit()) return false;
	
	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;
	
	return SteamUGC()->RemoveItemKeyValueTags(updateHandle64, (char*)keyStr);
}
DEFINE_PRIM(_BOOL, remove_ugcitem_key_value_tags, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_ugcitem_visibility)(vbyte *updateHandle, int visibility) {
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	ERemoteStoragePublishedFileVisibility visibilityEnum = static_cast<ERemoteStoragePublishedFileVisibility>(visibility);

	return SteamUGC()->SetItemVisibility(updateHandle64, visibilityEnum);
}
DEFINE_PRIM(_BOOL, set_ugcitem_visibility, _BYTES _I32);

HL_PRIM bool HL_NAME(set_ugcitem_content)(vbyte *updateHandle, vbyte *path){
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	return SteamUGC()->SetItemContent(updateHandle64, (char*)path);
}
DEFINE_PRIM(_BOOL, set_ugcitem_content, _BYTES _BYTES);

HL_PRIM bool HL_NAME(set_ugcitem_preview_image)(vbyte *updateHandle, vbyte *path){
	if (!CheckInit()) return false;

	// Create uint64 from the string.
	uint64 updateHandle64;
	std::istringstream handleStream((char*)updateHandle);
	if (!(handleStream >> updateHandle64))
		return false;

	return SteamUGC()->SetItemPreview(updateHandle64, (char*)path);
}
DEFINE_PRIM(_BOOL, set_ugcitem_preview_image, _BYTES _BYTES);

HL_PRIM bool HL_NAME(create_ugcitem)(int id) {
	if (!CheckInit()) return false;

	s_callbackHandler->CreateUGCItem(id, k_EWorkshopFileTypeCommunity);
 	return true;
}
DEFINE_PRIM(_BOOL, create_ugcitem, _I32);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_AddRequiredTag(const char * handle, const char * tagName)
{
	if (!CheckInit()) return 0;
	
	UGCQueryHandle_t u64Handle = strtoull(handle, NULL, 0);
	
	bool result = SteamUGC()->AddRequiredTag(u64Handle, tagName);
	return result;
}
DEFINE_PRIME2(SteamWrap_AddRequiredTag);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_AddRequiredKeyValueTag(const char * handle, const char * pKey, const char * pValue)
{
	if (!CheckInit()) return 0;
	
	UGCQueryHandle_t u64Handle = strtoull(handle, NULL, 0);
	
	bool result = SteamUGC()->AddRequiredKeyValueTag(u64Handle, pKey, pValue);
	return result;
}
DEFINE_PRIME3(SteamWrap_AddRequiredKeyValueTag);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_AddExcludedTag(const char * handle, const char * tagName)
{
	if (!CheckInit()) return 0;
	
	UGCQueryHandle_t u64Handle = strtoull(handle, NULL, 0);
	
	bool result = SteamUGC()->AddExcludedTag(u64Handle, tagName);
	return result;
}
DEFINE_PRIME2(SteamWrap_AddExcludedTag);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_SetReturnMetadata(const char * handle, int returnMetadata)
{
	if (!CheckInit()) return 0;
	
	UGCQueryHandle_t u64Handle = strtoull(handle, NULL, 0);
	
	bool result = SteamUGC()->SetReturnMetadata(u64Handle, returnMetadata == 1);
	return result;
}
DEFINE_PRIME2(SteamWrap_SetReturnMetadata);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_SetReturnKeyValueTags(const char * handle, int returnKeyValueTags)
{
	if (!CheckInit()) return 0;
	
	UGCQueryHandle_t u64Handle = strtoull(handle, NULL, 0);
	bool setValue = returnKeyValueTags == 1;
	
	bool result = SteamUGC()->SetReturnKeyValueTags(u64Handle, setValue);
	return result;
}
DEFINE_PRIME2(SteamWrap_SetReturnKeyValueTags);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(SetAchievement)(value name)
{
	if (!val_is_string(name) || !CheckInit())
		return false;

	SteamUserStats()->SetAchievement((char*)name);
	bool result = SteamUserStats()->StoreStats();

	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_SetAchievement, 1);

HL_PRIM value HL_NAME(GetAchievement)(value name)
{
  if (!val_is_string(name) || !CheckInit()) return false;
  bool achieved = false;
  SteamUserStats()->GetAchievement((char*)name, &achieved);
  return alloc_bool(achieved);
}
DEFINE_PRIM(SteamWrap_GetAchievement, 1);

HL_PRIM value HL_NAME(GetAchievementDisplayAttribute)(value name, value key)
{
  if (!val_is_string(name) || !val_is_string(key) || !CheckInit()) return alloc_string("");
  
  const char* result = SteamUserStats()->GetAchievementDisplayAttribute((char*)name, (char*)key);
  return alloc_string(result);
}
DEFINE_PRIM(SteamWrap_GetAchievementDisplayAttribute, 2);

HL_PRIM value HL_NAME(GetNumAchievements)()
{
  if (!CheckInit()) return alloc_int(0);
  
  uint32 count = SteamUserStats()->GetNumAchievements();
  return alloc_int((int)count);
}
DEFINE_PRIM(SteamWrap_GetNumAchievements, 0);

HL_PRIM value HL_NAME(GetAchievementName)(value index)
{
  if (!val_is_int(index) && !CheckInit()) return alloc_string("");
  const char* name = SteamUserStats()->GetAchievementName(val_int(index));
  return alloc_string(name);
}
DEFINE_PRIM(SteamWrap_GetAchievementName, 1);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(ClearAchievement)(value name)
{
	if (!val_is_string(name) || !CheckInit())
		return false;

	SteamUserStats()->ClearAchievement((char*)name);
	bool result = SteamUserStats()->StoreStats();

	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_ClearAchievement, 1);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(IndicateAchievementProgress)(value name, value numCurProgres, value numMaxProgress)
{
	if (!val_is_string(name) || !val_is_int(numCurProgres) || !val_is_int(numMaxProgress) || !CheckInit())
		return false;

	bool result = SteamUserStats()->IndicateAchievementProgress((char*)name, val_int(numCurProgres), val_int(numMaxProgress));

	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_IndicateAchievementProgress, 3);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(FindLeaderboard)(value name)
{
	if (!val_is_string(name) || !CheckInit())
		return false;

	s_callbackHandler->FindLeaderboard((char*)name);

 	return alloc_bool(true);
}
DEFINE_PRIM(SteamWrap_FindLeaderboard, 1);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(UploadScore)(value name, value score, value detail)
{
	if (!val_is_string(name) || !val_is_int(score) || !val_is_int(detail) || !CheckInit())
		return false;

	bool result = s_callbackHandler->UploadScore((char*)name, val_int(score), val_int(detail));
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_UploadScore, 3);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(DownloadScores)(value name, value numBefore, value numAfter)
{
	if (!val_is_string(name) || !val_is_int(numBefore) || !val_is_int(numAfter) || !CheckInit())
		return false;

	bool result = s_callbackHandler->DownloadScores((char*)name, val_int(numBefore), val_int(numAfter));
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_DownloadScores, 3);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(RequestGlobalStats)()
{
	if (!CheckInit())
		return false;

	s_callbackHandler->RequestGlobalStats();
	return alloc_bool(true);
}
DEFINE_PRIM(SteamWrap_RequestGlobalStats, 0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetGlobalStat)(value name)
{
	if (!val_is_string(name) || !CheckInit())
		return alloc_int(0);

	int64 val;
	SteamUserStats()->GetGlobalStat((char*)name, &val);

	return alloc_int((int)val);
}
DEFINE_PRIM(SteamWrap_GetGlobalStat, 1);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetPersonaName)()
{
	if(!CheckInit())
		return alloc_string("unknown");
	
	const char * persona = SteamFriends()->GetPersonaName();
	
	return alloc_string(persona);
}
DEFINE_PRIM(SteamWrap_GetPersonaName, 0);


//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetSteamID)()
{
	if(!CheckInit())
		return alloc_string("0");
	
	CSteamID userId = SteamUser()->GetSteamID();
	
	std::ostringstream returnData;
	returnData << userId.ConvertToUint64();
	
	return alloc_string(returnData.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetSteamID, 0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(RestartAppIfNecessary)(value appId)
{
	if (!val_is_int(appId))
		return false;

	bool result = SteamAPI_RestartAppIfNecessary(val_int(appId));
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_RestartAppIfNecessary, 1);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(IsOverlayEnabled)()
{
	bool result = SteamUtils()->IsOverlayEnabled();
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_IsOverlayEnabled, 0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(BOverlayNeedsPresent)()
{
	bool result = SteamUtils()->BOverlayNeedsPresent();
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_BOverlayNeedsPresent, 0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(IsSteamInBigPictureMode)()
{
	bool result = SteamUtils()->IsSteamInBigPictureMode();
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_IsSteamInBigPictureMode, 0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(IsSteamRunning)()
{
	bool result = SteamAPI_IsSteamRunning();
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_IsSteamRunning, 0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetCurrentGameLanguage)()
{
	const char* result = SteamApps()->GetCurrentGameLanguage();
	return alloc_string(result);
}
DEFINE_PRIM(SteamWrap_GetCurrentGameLanguage, 0);

//-----------------------------------------------------------------------------------------------------------

//NEW STEAM WORKSHOP---------------------------------------------------------------------------------------------

int SteamWrap_GetNumSubscribedItems(int dummy)
{
	if (!CheckInit()) return 0;
	int numItems = SteamUGC()->GetNumSubscribedItems();
	return numItems;
}
DEFINE_PRIME1(SteamWrap_GetNumSubscribedItems);

HL_PRIM value HL_NAME(GetSubscribedItems)()
{
	if (!CheckInit()) return alloc_string("");
	
	int numSubscribed = SteamUGC()->GetNumSubscribedItems();
	if(numSubscribed <= 0) return alloc_string("");
	PublishedFileId_t* pvecPublishedFileID = new PublishedFileId_t[numSubscribed];
	
	int result = SteamUGC()->GetSubscribedItems(pvecPublishedFileID, numSubscribed);
	
	std::ostringstream data;
	for(int i = 0; i < result; i++){
		if(i != 0){
			data << ",";
		}
		data << pvecPublishedFileID[i];
	}
	delete pvecPublishedFileID;
	
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetSubscribedItems, 0);

int SteamWrap_GetItemState(const char * publishedFileID)
{
	if (!CheckInit()) return 0;
	PublishedFileId_t nPublishedFileID = (PublishedFileId_t) strtoll(publishedFileID, NULL, 10);
	return SteamUGC()->GetItemState(nPublishedFileID);
}
DEFINE_PRIME1(SteamWrap_GetItemState);

HL_PRIM value HL_NAME(GetItemDownloadInfo)(value publishedFileID)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_string(publishedFileID)) return alloc_string("");
	
	PublishedFileId_t nPublishedFileID = (PublishedFileId_t) strtoll((char*)publishedFileID, NULL, 10);
	
	uint64 punBytesDownloaded;
	uint64 punBytesTotal;
	
	bool result = SteamUGC()->GetItemDownloadInfo(nPublishedFileID, &punBytesDownloaded, &punBytesTotal);
	
	if(result){
		std::ostringstream data;
		data << punBytesDownloaded;
		data << ",";
		data << punBytesTotal;
		return alloc_string(data.str().c_str());
	}
	
	return alloc_string("0,0");
}
DEFINE_PRIM(SteamWrap_GetItemDownloadInfo, 1);

int SteamWrap_DownloadItem(const char * publishedFileID, int highPriority)
{
	if (!CheckInit()) return false;
	PublishedFileId_t nPublishedFileID = (PublishedFileId_t) strtoll(publishedFileID, NULL, 10);
	
	bool bHighPriority = highPriority == 1;
	bool result = SteamUGC()->DownloadItem(nPublishedFileID, bHighPriority);
	return result;
}
DEFINE_PRIME2(SteamWrap_DownloadItem);

HL_PRIM value HL_NAME(GetItemInstallInfo)(value publishedFileID, value maxFolderPathLength)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_string(publishedFileID)) return alloc_string("");
	if (!val_is_int(maxFolderPathLength)) return alloc_string("");
	
	PublishedFileId_t nPublishedFileID = (PublishedFileId_t) strtoll((char*)publishedFileID, NULL, 10);
	
	uint64 punSizeOnDisk;
	uint32 punTimeStamp;
	uint32 cchFolderSize = (uint32) val_int(maxFolderPathLength);
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
		return alloc_string(data.str().c_str());
	}
	
	return alloc_string("0||0|");
}
DEFINE_PRIM(SteamWrap_GetItemInstallInfo, 2);

/*
HL_PRIM value HL_NAME(CreateQueryUserUGCRequest)(value accountID, value listType, value matchingUGCType, value sortOrder, value creatorAppID, value consumerAppID, value page)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_int(accountID)) return alloc_string("");
	if (!val_is_int(listType)) return alloc_string("");
	if (!val_is_int(matchingUGCType)) return alloc_string("");
	if (!val_is_int(sortOrder)) return alloc_string("");
	if (!val_is_int(creatorAppID)) return alloc_string("");
	if (!val_is_int(consumerAppID)) return alloc_string("");
	if (!val_is_int(page)) return alloc_string("");
	
	AccountID_t unAccountID = val_int(accountID);
	EUserUGCList eListType = val_int(listType);
	EUGCMatchingUGCType eMatchingUGCType = val_int(matchingUGCType);
	EUserUGCListSortOrder eSortOrder = val_int(sortOrder);
	AppID_t nCreatorAppID = val_int(creatorAppID);
	AppId_t nConsumerAppID = val_int(consumerAppID);
	uint32 page = val_int(page);
	
	UGCQueryHandle_t result = SteamUGC()->SteamWrap_CreateQueryUserUGCRequest(unAccountID, eListType, eMatchingUGCType, eSortOrder, nCreatorAppID, nConsumerAppId, unPage);
	
	std:ostringstream data;
	data << result;
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_CreateQueryUserUGCRequest, 7);
*/

HL_PRIM value HL_NAME(CreateQueryAllUGCRequest)(value queryType, value matchingUGCType, value creatorAppID, value consumerAppID, value page)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_int(queryType)) return alloc_string("");
	if (!val_is_int(matchingUGCType)) return alloc_string("");
	if (!val_is_int(creatorAppID)) return alloc_string("");
	if (!val_is_int(consumerAppID)) return alloc_string("");
	if (!val_is_int(page)) return alloc_string("");
	
	EUGCQuery eQueryType = (EUGCQuery) val_int(queryType);
	EUGCMatchingUGCType eMatchingUGCType = (EUGCMatchingUGCType) val_int(matchingUGCType);
	AppId_t nCreatorAppID = val_int(creatorAppID);
	AppId_t nConsumerAppID = val_int(consumerAppID);
	uint32 unPage = val_int(page);
	
	UGCQueryHandle_t result = SteamUGC()->CreateQueryAllUGCRequest(eQueryType, eMatchingUGCType, nCreatorAppID, nConsumerAppID, unPage);
	
	std::ostringstream data;
	data << result;
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_CreateQueryAllUGCRequest, 5);

HL_PRIM value HL_NAME(CreateQueryUGCDetailsRequest)(value fileIDs)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_string(fileIDs)) return alloc_string("");
	uint32 unNumPublishedFileIDs = 0;
	PublishedFileId_t * pvecPublishedFileID = getUint64Array((char*)fileIDs, &unNumPublishedFileIDs);
	
	UGCQueryHandle_t result = SteamUGC()->CreateQueryUGCDetailsRequest(pvecPublishedFileID, unNumPublishedFileIDs);
	
	std::ostringstream data;
	data << result;
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_CreateQueryUGCDetailsRequest, 1);


void SteamWrap_SendQueryUGCRequest(const char * cHandle)
{
	if (!CheckInit()) return;
	
	UGCQueryHandle_t handle = strtoull(cHandle, NULL, 0);
	
	s_callbackHandler->SendQueryUGCRequest(handle);
}
DEFINE_PRIME1v(SteamWrap_SendQueryUGCRequest);


int SteamWrap_GetQueryUGCNumKeyValueTags(const char * cHandle, int iIndex)
{
	if (!CheckInit()) return 0;
	
	UGCQueryHandle_t handle = strtoull(cHandle, NULL, 0);
	uint32 index = iIndex;
	
	return SteamUGC()->GetQueryUGCNumKeyValueTags(handle, index);
}
DEFINE_PRIME2(SteamWrap_GetQueryUGCNumKeyValueTags);

int SteamWrap_ReleaseQueryUGCRequest(const char * cHandle)
{
	if (!CheckInit()) return false;
	UGCQueryHandle_t handle = strtoull(cHandle, NULL, 0);
	return SteamUGC()->ReleaseQueryUGCRequest(handle);
}
DEFINE_PRIME1v(SteamWrap_ReleaseQueryUGCRequest);

HL_PRIM value HL_NAME(GetQueryUGCKeyValueTag)(value cHandle, value iIndex, value iKeyValueTagIndex, value keySize, value valueSize)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_string(cHandle)) return alloc_string("");
	if (!val_is_int(iIndex)) return alloc_string("");
	if (!val_is_int(iKeyValueTagIndex)) return alloc_string("");
	if (!val_is_int(keySize)) return alloc_string("");
	if (!val_is_int(valueSize)) return alloc_string("");
	
	UGCQueryHandle_t handle = strtoull((char*)cHandle, NULL, 0);
	uint32 index = val_int(iIndex);
	uint32 keyValueTagIndex = val_int(iKeyValueTagIndex);
	uint32 cchKeySize = val_int(keySize);
	uint32 cchValueSize = val_int(valueSize);
	
	char *pchKey = new char[cchKeySize];
	char *pchValue = new char[cchValueSize];
	
	SteamUGC()->GetQueryUGCKeyValueTag(handle, index, keyValueTagIndex, pchKey, cchKeySize, pchValue, cchValueSize);
	
	std::ostringstream data;
	data << pchKey << "=" << pchValue;
	
	delete pchKey;
	delete pchValue;
	
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetQueryUGCKeyValueTag, 5);

HL_PRIM value HL_NAME(GetQueryUGCMetadata)(value sHandle, value iIndex, value iMetaDataSize)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_string(sHandle)) return alloc_string("");
	if (!val_is_int(iIndex)) return alloc_string("");
	if (!val_is_int(iMetaDataSize)) return alloc_string("");
	
	UGCQueryHandle_t handle = strtoull((char*)sHandle, NULL, 0);
	
	
	uint32 cchMetadatasize = val_int(iMetaDataSize);
	char * pchMetadata = new char[cchMetadatasize];
	
	uint32 index = val_int(iIndex);
	
	SteamUGC()->GetQueryUGCMetadata(handle, index, pchMetadata, cchMetadatasize);
	
	std::ostringstream data;
	data << pchMetadata;
	
	delete pchMetadata;
	
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetQueryUGCMetadata, 3);

HL_PRIM value HL_NAME(GetQueryUGCResult)(value sHandle, value iIndex)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_string(sHandle)) return alloc_string("");
	if (!val_is_int(iIndex)) return alloc_string("");
	
	UGCQueryHandle_t handle = strtoull((char*)sHandle, NULL, 0);
	
	uint32 index = val_int(iIndex);
	
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
	
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetQueryUGCResult, 2);

//OLD STEAM WORKSHOP---------------------------------------------------------------------------------------------

HL_PRIM value HL_NAME(GetUGCDownloadProgress)(value contentHandle)
{
	if (!CheckInit()) return alloc_string("");
	if (!val_is_string(contentHandle)) return alloc_string("");
	
	uint64 u64Handle = strtoll((char*)contentHandle, NULL, 10);
	
	int32 pnBytesDownloaded = 0;
	int32 pnBytesExpected = 0;
	
	SteamRemoteStorage()->GetUGCDownloadProgress(u64Handle, &pnBytesDownloaded, &pnBytesExpected);
	
	std::ostringstream data;
	data << pnBytesDownloaded << "," << pnBytesExpected;
	
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetUGCDownloadProgress,1);

void SteamWrap_EnumerateUserSharedWorkshopFiles(const char * steamIDStr, int startIndex, const char * requiredTagsStr, const char * excludedTagsStr)
{
	if(!CheckInit()) return;
	
	//Reconstruct the steamID from the string representation
	uint64 u64SteamID = strtoll(steamIDStr, NULL, 10);
	CSteamID steamID = u64SteamID;
	
	uint32 unStartIndex = (uint32) startIndex;
	
	//Construct the string arrays from the comma-delimited strings
	SteamParamStringArray_t * requiredTags = getSteamParamStringArray(requiredTagsStr);
	SteamParamStringArray_t * excludedTags = getSteamParamStringArray(excludedTagsStr);
	
	//make the actual call
	s_callbackHandler->EnumerateUserSharedWorkshopFiles(steamID, startIndex, requiredTags, excludedTags);
	
	//clean up requiredTags & excludedTags:
	deleteSteamParamStringArray(requiredTags);
	deleteSteamParamStringArray(excludedTags);
}
DEFINE_PRIME4v(SteamWrap_EnumerateUserSharedWorkshopFiles);

void SteamWrap_EnumerateUserPublishedFiles(int startIndex)
{
	if(!CheckInit()) return;
	uint32 unStartIndex = (uint32) startIndex;
	s_callbackHandler->EnumerateUserPublishedFiles(unStartIndex);
}
DEFINE_PRIME1v(SteamWrap_EnumerateUserPublishedFiles);

void SteamWrap_EnumerateUserSubscribedFiles(int startIndex)
{
	if(!CheckInit());
	uint32 unStartIndex = (uint32) startIndex;
	s_callbackHandler->EnumerateUserSubscribedFiles(unStartIndex);
}
DEFINE_PRIME1v(SteamWrap_EnumerateUserSubscribedFiles);

void SteamWrap_GetPublishedFileDetails(const char * fileId, int maxSecondsOld)
{
	if(!CheckInit());
	
	uint64 u64FileID = strtoull(fileId, NULL, 0);
	uint32 u32MaxSecondsOld = maxSecondsOld;
	
	s_callbackHandler->GetPublishedFileDetails(u64FileID, u32MaxSecondsOld);
}
DEFINE_PRIME2v(SteamWrap_GetPublishedFileDetails);

void SteamWrap_UGCDownload(const char * handle, int priority)
{
	if(!CheckInit());
	
	uint64 u64Handle = strtoull(handle, NULL, 0);
	uint32 u32Priority = (uint32) priority;
	
	s_callbackHandler->UGCDownload(u64Handle, u32Priority);
}
DEFINE_PRIME2v(SteamWrap_UGCDownload);

HL_PRIM value HL_NAME(UGCRead)(value handle, value bytesToRead, value offset, value readAction)
{
	if(!CheckInit()             ||
	   !val_is_string(handle)   ||
	   !val_is_int(bytesToRead) ||
	   !val_is_int(offset)      ||
	   !val_is_int(readAction)) return alloc_string("");
	
	uint64 u64Handle = strtoull((char*)handle, NULL, 0);
	int32 cubDataToRead = (int32) val_int(bytesToRead);
	uint32 cOffset = (uint32) val_int(offset);
	EUGCReadAction eAction = (EUGCReadAction) val_int(readAction);
	
	if(u64Handle == 0 || cubDataToRead == 0) return alloc_string("");
	
	unsigned char *data = (unsigned char *)malloc(cubDataToRead);
	int result = SteamRemoteStorage()->UGCRead(u64Handle, data, cubDataToRead, cOffset, eAction);
	
	value returnValue = bytes_to_hx(data,result);
	
	free(data);
	
	return returnValue;
}
DEFINE_PRIM(SteamWrap_UGCRead,4);

//-----------------------------------------------------------------------------------------------------------

//STEAM CLOUD------------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_GetFileCount(int dummy)
{
	int fileCount = SteamRemoteStorage()->GetFileCount();
	return fileCount;
}
DEFINE_PRIME1(SteamWrap_GetFileCount);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_GetFileSize(const char * fileName)
{
	int fileSize = SteamRemoteStorage()->GetFileSize(fileName);
	return fileSize;
}
DEFINE_PRIME1(SteamWrap_GetFileSize);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_FileExists(const char * fileName)
{
	bool exists = SteamRemoteStorage()->FileExists(fileName);
	return exists;
}
DEFINE_PRIME1(SteamWrap_FileExists);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(FileRead)(value fileName)
{
	if (!val_is_string(fileName) || !CheckInit())
		return alloc_int(0);
	
	const char * fName = (char*)fileName;
	
	bool exists = SteamRemoteStorage()->FileExists(fName);
	if(!exists) return alloc_int(0);
	
	int length = SteamRemoteStorage()->GetFileSize(fName);
	
	char *bytesData = (char *)malloc(length);
	int32 result = SteamRemoteStorage()->FileRead(fName, bytesData, length);
	
	value returnValue = alloc_string(bytesData);
	
	free(bytesData);
	return returnValue;
}
DEFINE_PRIM(SteamWrap_FileRead, 1);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(FileWrite)(value fileName, value haxeBytes)
{
	if (!val_is_string(fileName) || !CheckInit())
		return false;
	
	CffiBytes bytes = getByteData(haxeBytes);
	if(bytes.data == 0)
		return false;
	
	bool result = SteamRemoteStorage()->FileWrite((char*)fileName, bytes.data, bytes.length);
	
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_FileWrite, 2);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_FileDelete(const char * fileName)
{
	bool result = SteamRemoteStorage()->FileDelete(fileName);
	return result;
}
DEFINE_PRIME1(SteamWrap_FileDelete);

//-----------------------------------------------------------------------------------------------------------
void SteamWrap_FileShare(const char * fileName)
{
	s_callbackHandler->FileShare(fileName);
}
DEFINE_PRIME1v(SteamWrap_FileShare);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_IsCloudEnabledForApp(int dummy)
{
	int result = SteamRemoteStorage()->IsCloudEnabledForApp();
	return result;
}
DEFINE_PRIME1(SteamWrap_IsCloudEnabledForApp);

//-----------------------------------------------------------------------------------------------------------
void SteamWrap_SetCloudEnabledForApp(int enabled)
{
	SteamRemoteStorage()->SetCloudEnabledForApp(enabled);
}
DEFINE_PRIME1v(SteamWrap_SetCloudEnabledForApp);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetQuota)()
{
	uint64 total = 0;
	uint64 available = 0;
	
	//convert uint64 handle to string
	std::ostringstream data;
	data << total << "," << available;
	
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetQuota,0);

//-----------------------------------------------------------------------------------------------------------

//STEAM CONTROLLER-------------------------------------------------------------------------------------------

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(InitControllers)()
{
	if (!SteamController()) return false;

	bool result = SteamController()->Init();
	
	if (result)
	{
		mapControllers.init();
		
		analogActionData.eMode = k_EControllerSourceMode_None;
		analogActionData.x = 0.0;
		analogActionData.y = 0.0;
		analogActionData.bActive = false;
	}
	
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_InitControllers,0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(ShutdownControllers)()
{
	bool result = SteamController()->Shutdown();
	if (result)
	{
		mapControllers.init();
	}
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_ShutdownControllers,0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(ShowBindingPanel)(value controllerHandle)
{
	if(!val_is_int(controllerHandle)) 
		return false;
	
	int i_handle = val_int(controllerHandle);
	
	ControllerHandle_t c_handle = i_handle != -1 ? mapControllers.get(i_handle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	
	bool result = SteamController()->ShowBindingPanel(c_handle);
	
	return alloc_bool(result);
}
DEFINE_PRIM(SteamWrap_ShowBindingPanel, 1);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_ShowGamepadTextInput(int inputMode, int lineMode, const char * description, int charMax, const char * existingText)
{
	uint32 u_charMax = charMax;
	
	EGamepadTextInputMode eInputMode = static_cast<EGamepadTextInputMode>(inputMode);
	EGamepadTextInputLineMode eLineInputMode = static_cast<EGamepadTextInputLineMode>(lineMode);
	
	int result = SteamUtils()->ShowGamepadTextInput(eInputMode, eLineInputMode, description, u_charMax, existingText);
	return result;

}
DEFINE_PRIME5(SteamWrap_ShowGamepadTextInput);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetEnteredGamepadTextInput)()
{
	uint32 length = SteamUtils()->GetEnteredGamepadTextLength();
	char *pchText = (char *)malloc(length);
	bool result = SteamUtils()->GetEnteredGamepadTextInput(pchText, length);
	if(result)
	{
		value returnValue = alloc_string(pchText);
		free(pchText);
		return returnValue;
	}
	free(pchText);
	return alloc_string("");

}
DEFINE_PRIM(SteamWrap_GetEnteredGamepadTextInput, 0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetConnectedControllers)()
{
	SteamController()->RunFrame();
	
	ControllerHandle_t handles[STEAM_CONTROLLER_MAX_COUNT];
	int result = SteamController()->GetConnectedControllers(handles);
	
	std::ostringstream returnData;
	
	//store the handles locally and pass back a string representing an int array of unique index lookup values
	
	for(int i = 0; i < result; i++)
	{
		int index = -1;
		
		if(false == mapControllers.exists(handles[i]))
		{
			index = mapControllers.add(handles[i]);
		}
		else
		{
			index = mapControllers.get(handles[i]);
		}
		
		if(index != -1)
		{
			returnData << index;
			if(i != result-1)
			{
				returnData << ",";
			}
		}
	}
	
	return alloc_string(returnData.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetConnectedControllers,0);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_GetActionSetHandle(const char * actionSetName)
{

	ControllerActionSetHandle_t handle = SteamController()->GetActionSetHandle(actionSetName);
	return handle;
}
DEFINE_PRIME1(SteamWrap_GetActionSetHandle);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_GetDigitalActionHandle(const char * actionName)
{

	return SteamController()->GetDigitalActionHandle(actionName);
}
DEFINE_PRIME1(SteamWrap_GetDigitalActionHandle);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_GetAnalogActionHandle(const char * actionName)
{

	ControllerAnalogActionHandle_t handle = SteamController()->GetAnalogActionHandle(actionName);
	return handle;
}
DEFINE_PRIME1(SteamWrap_GetAnalogActionHandle);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_GetDigitalActionData(int controllerHandle, int actionHandle)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ControllerDigitalActionHandle_t a_handle = actionHandle;
	
	ControllerDigitalActionData_t data = SteamController()->GetDigitalActionData(c_handle, a_handle);
	
	int result = 0;
	
	//Take both bools and pack them into an int
	
	if(data.bState) {
		result |= 0x1;
	}
	
	if(data.bActive) {
		result |= 0x10;
	}
	
	return result;
}
DEFINE_PRIME2(SteamWrap_GetDigitalActionData);


//-----------------------------------------------------------------------------------------------------------
//stashes the requested analog action data in local state and returns the bActive member value
//you need to immediately call _eMode(), _x(), and _y() to get the rest

int SteamWrap_GetAnalogActionData(int controllerHandle, int actionHandle)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ControllerAnalogActionHandle_t a_handle = actionHandle;
	
	analogActionData = SteamController()->GetAnalogActionData(c_handle, a_handle);
	
	return analogActionData.bActive;
}
DEFINE_PRIME2(SteamWrap_GetAnalogActionData);

int SteamWrap_GetAnalogActionData_eMode(int dummy)
{
	return analogActionData.eMode;
}
DEFINE_PRIME1(SteamWrap_GetAnalogActionData_eMode);

float SteamWrap_GetAnalogActionData_x(int dummy)
{
	return analogActionData.x;
}
DEFINE_PRIME1(SteamWrap_GetAnalogActionData_x);

float SteamWrap_GetAnalogActionData_y(int dummy)
{
	return analogActionData.y;
}
DEFINE_PRIME1(SteamWrap_GetAnalogActionData_y);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetDigitalActionOrigins)(value controllerHandle, value actionSetHandle, value digitalActionHandle)
{
	ControllerHandle_t c_handle              = mapControllers.get(val_int(controllerHandle));
	ControllerActionSetHandle_t s_handle     = val_int(actionSetHandle);
	ControllerDigitalActionHandle_t a_handle = val_int(digitalActionHandle);
	
	EControllerActionOrigin origins[STEAM_CONTROLLER_MAX_ORIGINS];
	
	//Initialize the whole thing to None to avoid garbage
	for(int i = 0; i < STEAM_CONTROLLER_MAX_ORIGINS; i++) {
		origins[i] = k_EControllerActionOrigin_None;
	}
	
	int result = SteamController()->GetDigitalActionOrigins(c_handle, s_handle, a_handle, origins);
	
	std::ostringstream data;
	
	data << result << ",";
	
	for(int i = 0; i < STEAM_CONTROLLER_MAX_ORIGINS; i++) {
		data << origins[i];
		if(i != STEAM_CONTROLLER_MAX_ORIGINS-1){
			data << ",";
		}
	}
	
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetDigitalActionOrigins,3);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetAnalogActionOrigins)(value controllerHandle, value actionSetHandle, value analogActionHandle)
{
	ControllerHandle_t c_handle              = mapControllers.get(val_int(controllerHandle));
	ControllerActionSetHandle_t s_handle     = val_int(actionSetHandle);
	ControllerAnalogActionHandle_t a_handle  = val_int(analogActionHandle);
	
	EControllerActionOrigin origins[STEAM_CONTROLLER_MAX_ORIGINS];
	
	//Initialize the whole thing to None to avoid garbage
	for(int i = 0; i < STEAM_CONTROLLER_MAX_ORIGINS; i++) {
		origins[i] = k_EControllerActionOrigin_None;
	}
	
	int result = SteamController()->GetAnalogActionOrigins(c_handle, s_handle, a_handle, origins);
	
	std::ostringstream data;
	
	data << result << ",";
	
	for(int i = 0; i < STEAM_CONTROLLER_MAX_ORIGINS; i++) {
		data << origins[i];
		if(i != STEAM_CONTROLLER_MAX_ORIGINS-1){
			data << ",";
		}
	}
	
	return alloc_string(data.str().c_str());
}
DEFINE_PRIM(SteamWrap_GetAnalogActionOrigins,3);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetGlyphForActionOrigin)(value origin)
{
	if (!val_is_int(origin) || !CheckInit())
	{
		return alloc_string("none");
	}
	
	int iOrigin = val_int(origin);
	if (iOrigin >= k_EControllerActionOrigin_Count)
	{
		return alloc_string("none");
	}
	
	EControllerActionOrigin eOrigin = static_cast<EControllerActionOrigin>(iOrigin);
	
	const char * result = SteamController()->GetGlyphForActionOrigin(eOrigin);
	return alloc_string(result);
}
DEFINE_PRIM(SteamWrap_GetGlyphForActionOrigin,1);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetStringForActionOrigin)(value origin)
{
	if (!val_is_int(origin) || !CheckInit())
	{
		return alloc_string("unknown");
	}
	
	int iOrigin = val_int(origin);
	if (iOrigin >= k_EControllerActionOrigin_Count)
	{
		return alloc_string("unknown");
	}
	
	EControllerActionOrigin eOrigin = static_cast<EControllerActionOrigin>(iOrigin);
	
	const char * result = SteamController()->GetStringForActionOrigin(eOrigin);
	return alloc_string(result);
}
DEFINE_PRIM(SteamWrap_GetStringForActionOrigin,1);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_ActivateActionSet(int controllerHandle, int actionSetHandle)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ControllerActionSetHandle_t a_handle = actionSetHandle;
	
	SteamController()->ActivateActionSet(c_handle, a_handle);
	
	return true;
}
DEFINE_PRIME2(SteamWrap_ActivateActionSet);

//-----------------------------------------------------------------------------------------------------------
int SteamWrap_GetCurrentActionSet(int controllerHandle)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ControllerActionSetHandle_t a_handle = SteamController()->GetCurrentActionSet(c_handle);
	
	return a_handle;
}
DEFINE_PRIME1(SteamWrap_GetCurrentActionSet);

void SteamWrap_TriggerHapticPulse(int controllerHandle, int targetPad, int durationMicroSec)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ESteamControllerPad eTargetPad;
	switch(targetPad)
	{
		case 0:  eTargetPad = k_ESteamControllerPad_Left;
		case 1:  eTargetPad = k_ESteamControllerPad_Right;
		default: eTargetPad = k_ESteamControllerPad_Left;
	}
	unsigned short usDurationMicroSec = durationMicroSec;
	
	SteamController()->TriggerHapticPulse(c_handle, eTargetPad, usDurationMicroSec);
}
DEFINE_PRIME3v(SteamWrap_TriggerHapticPulse);

void SteamWrap_TriggerRepeatedHapticPulse(int controllerHandle, int targetPad, int durationMicroSec, int offMicroSec, int repeat, int flags)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ESteamControllerPad eTargetPad;
	switch(targetPad)
	{
		case 0:  eTargetPad = k_ESteamControllerPad_Left;
		case 1:  eTargetPad = k_ESteamControllerPad_Right;
		default: eTargetPad = k_ESteamControllerPad_Left;
	}
	unsigned short usDurationMicroSec = durationMicroSec;
	unsigned short usOffMicroSec = offMicroSec;
	unsigned short unRepeat = repeat;
	unsigned short nFlags = flags;
	
	SteamController()->TriggerRepeatedHapticPulse(c_handle, eTargetPad, usDurationMicroSec, usOffMicroSec, unRepeat, nFlags);
}
DEFINE_PRIME6v(SteamWrap_TriggerRepeatedHapticPulse);

void SteamWrap_TriggerVibration(int controllerHandle, int leftSpeed, int rightSpeed)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	SteamController()->TriggerVibration(c_handle, (unsigned short)leftSpeed, (unsigned short)rightSpeed);
}
DEFINE_PRIME3v(SteamWrap_TriggerVibration);

void SteamWrap_SetLEDColor(int controllerHandle, int r, int g, int b, int flags)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	SteamController()->SetLEDColor(c_handle, (uint8)r, (uint8)g, (uint8)b, (unsigned int) flags);
}
DEFINE_PRIME5v(SteamWrap_SetLEDColor);

//-----------------------------------------------------------------------------------------------------------
//stashes the requested motion data in local state
//you need to immediately call _rotQuatX/Y/Z/W, _posAccelX/Y/Z, _rotVelX/Y/Z to get the rest

void SteamWrap_GetMotionData(int controllerHandle)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	motionData = SteamController()->GetMotionData(c_handle);
}
DEFINE_PRIME1v(SteamWrap_GetMotionData);

int SteamWrap_GetMotionData_rotQuatX(int dummy)
{
	return motionData.rotQuatX;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_rotQuatX);

int SteamWrap_GetMotionData_rotQuatY(int dummy)
{
	return motionData.rotQuatY;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_rotQuatY);

int SteamWrap_GetMotionData_rotQuatZ(int dummy)
{
	return motionData.rotQuatZ;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_rotQuatZ);

int SteamWrap_GetMotionData_rotQuatW(int dummy)
{
	return motionData.rotQuatW;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_rotQuatW);

int SteamWrap_GetMotionData_posAccelX(int dummy)
{
	return motionData.posAccelX;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_posAccelX);

int SteamWrap_GetMotionData_posAccelY(int dummy)
{
	return motionData.posAccelY;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_posAccelY);

int SteamWrap_GetMotionData_posAccelZ(int dummy)
{
	return motionData.posAccelZ;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_posAccelZ);

int SteamWrap_GetMotionData_rotVelX(int dummy)
{
	return motionData.rotVelX;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_rotVelX);

int SteamWrap_GetMotionData_rotVelY(int dummy)
{
	return motionData.rotVelY;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_rotVelY);

int SteamWrap_GetMotionData_rotVelZ(int dummy)
{
	return motionData.rotVelZ;
}
DEFINE_PRIME1(SteamWrap_GetMotionData_rotVelZ);

int SteamWrap_ShowDigitalActionOrigins(int controllerHandle, int digitalActionHandle, float scale, float xPosition, float yPosition)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	return SteamController()->ShowDigitalActionOrigins(c_handle, digitalActionHandle, scale, xPosition, yPosition);
}
DEFINE_PRIME5(SteamWrap_ShowDigitalActionOrigins);

int SteamWrap_ShowAnalogActionOrigins(int controllerHandle, int analogActionHandle, float scale, float xPosition, float yPosition)
{
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	return SteamController()->ShowAnalogActionOrigins(c_handle, analogActionHandle, scale, xPosition, yPosition);
}
DEFINE_PRIME5(SteamWrap_ShowAnalogActionOrigins);



//---getters for constants----------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetControllerMaxCount)()
{
	return alloc_int(STEAM_CONTROLLER_MAX_COUNT);
}
DEFINE_PRIM(SteamWrap_GetControllerMaxCount,0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetControllerMaxAnalogActions)()
{
	return alloc_int(STEAM_CONTROLLER_MAX_ANALOG_ACTIONS);
}
DEFINE_PRIM(SteamWrap_GetControllerMaxAnalogActions,0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetControllerMaxDigitalActions)()
{
	return alloc_int(STEAM_CONTROLLER_MAX_DIGITAL_ACTIONS);
}
DEFINE_PRIM(SteamWrap_GetControllerMaxDigitalActions,0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetControllerMaxOrigins)()
{
	return alloc_int(STEAM_CONTROLLER_MAX_ORIGINS);
}
DEFINE_PRIM(SteamWrap_GetControllerMaxOrigins,0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetControllerMinAnalogActionData)()
{
	return alloc_float(STEAM_CONTROLLER_MIN_ANALOG_ACTION_DATA);
}
DEFINE_PRIM(SteamWrap_GetControllerMinAnalogActionData,0);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM value HL_NAME(GetControllerMaxAnalogActionData)()
{
	return alloc_float(STEAM_CONTROLLER_MAX_ANALOG_ACTION_DATA);
}
DEFINE_PRIM(SteamWrap_GetControllerMaxAnalogActionData,0);

//-----------------------------------------------------------------------------------------------------------

void mylib_main()
{
    // Initialization code goes here
}
DEFINE_ENTRY_POINT(mylib_main);


} // extern "C"

