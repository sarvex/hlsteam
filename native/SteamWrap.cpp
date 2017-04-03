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
static void SendEvent(event_type type, bool success, const char *data) {
	if (!g_eventHandler) return;
	if (g_eventHandler->hasValue)
		((void(*)(void*, event_type, bool, vbyte*))g_eventHandler->fun)(g_eventHandler->value, type, success, (vbyte*)data);
	else
		((void(*)(event_type, bool, vbyte*))g_eventHandler->fun)(type, success, (vbyte*)data);
}


template< class T >
class CClosureCallResult : public CCallResult<CClosureCallResult<T>,T> {
	vclosure *closure;
	void (*on_result)( vclosure *, T *, bool);
public:
	CClosureCallResult( vclosure *cval, void (*on_result)( vclosure *t, T*, bool) ) {
		this->closure = cval;
		this->on_result = on_result;
		hl_add_root(&closure);
	}
	~CClosureCallResult() {
		hl_remove_root(&closure);
	}
	void OnResult( T *result, bool onIOError ) {
		on_result(closure,result,onIOError);
		delete this;}
};

#define ASYNC_CALL(steam_call, type, on_result) \
	CClosureCallResult<type> *m_call = new CClosureCallResult<type>(closure,on_result); \
	m_call->Set(steam_call, m_call, &CClosureCallResult<type>::OnResult);

#define _CRESULT	_ABSTRACT(steam_call_result)
#define _CALLB(T)	_FUN(_VOID, T _BOOL)

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
		// TODO gc_root
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
	// TODO gc_root
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

//-----------------------------------------------------------------------------------------------------------

HL_PRIM bool HL_NAME(set_achievement)(vbyte *name){
	if (!CheckInit()) return false;

	SteamUserStats()->SetAchievement((char*)name);
	return SteamUserStats()->StoreStats();
}
DEFINE_PRIM(_BOOL, set_achievement, _BYTES);

HL_PRIM bool HL_NAME(get_achievement)(vbyte *name) {
  if (!CheckInit()) return false;
  bool achieved = false;
  SteamUserStats()->GetAchievement((char*)name, &achieved);
  return achieved;
}
DEFINE_PRIM(_BOOL, get_achievement, _BYTES);

HL_PRIM vbyte *HL_NAME(get_achievement_display_attribute)(vbyte *name, vbyte *key){
  if (!CheckInit()) return (vbyte*)"";
  return (vbyte*)SteamUserStats()->GetAchievementDisplayAttribute((char*)name, (char*)key);
}
DEFINE_PRIM(_BYTES, get_achievement_display_attribute, _BYTES _BYTES);

HL_PRIM int HL_NAME(get_num_achievements)(){
  if (!CheckInit()) return 0;
  return (int)SteamUserStats()->GetNumAchievements();
}
DEFINE_PRIM(_I32, get_num_achievements, _NO_ARG);

HL_PRIM vbyte *HL_NAME(get_achievement_name)(int index){
  if (!CheckInit()) return (vbyte*)"";
  return (vbyte*)SteamUserStats()->GetAchievementName(index);
}
DEFINE_PRIM(_BYTES, get_achievement_name, _I32);

HL_PRIM bool HL_NAME(clear_achievement)(vbyte *name){
	if (!CheckInit()) return false;
	SteamUserStats()->ClearAchievement((char*)name);
	return SteamUserStats()->StoreStats();
}
DEFINE_PRIM(_BOOL, clear_achievement, _BYTES);

HL_PRIM bool HL_NAME(indicate_achievement_progress)(vbyte *name, int numCurProgres, int numMaxProgress){
	if (!CheckInit()) return false;
	return SteamUserStats()->IndicateAchievementProgress((char*)name, numCurProgres, numMaxProgress);
}
DEFINE_PRIM(_BOOL, indicate_achievement_progress, _BYTES _I32 _I32);

//-----------------------------------------------------------------------------------------------------------

HL_PRIM bool HL_NAME(find_leaderboard)(vbyte *name) {
	if (!CheckInit()) return false;
	s_callbackHandler->FindLeaderboard((char*)name);
 	return true;
}
DEFINE_PRIM(_BOOL, find_leaderboard, _BYTES);

HL_PRIM bool HL_NAME(upload_score)(vbyte *name, int score, int detail){
	if (!CheckInit()) return false;
	return s_callbackHandler->UploadScore((char*)name, score, detail);
}
DEFINE_PRIM(_BOOL, upload_score, _BYTES _I32 _I32);

HL_PRIM bool HL_NAME(download_scores)(vbyte *name, int numBefore, int numAfter) {
	if (!CheckInit()) return false;
	return s_callbackHandler->DownloadScores((char*)name, numBefore, numAfter);
}
DEFINE_PRIM(_BOOL, download_scores, _BYTES _I32 _I32);

HL_PRIM bool HL_NAME(request_global_stats)() {
	if (!CheckInit()) return false;
	s_callbackHandler->RequestGlobalStats();
	return true;
}
DEFINE_PRIM(_BOOL, request_global_stats, _NO_ARG);

HL_PRIM int HL_NAME(get_global_stat)(vbyte *name){
	if (!CheckInit()) return 0;

	int64 val;
	SteamUserStats()->GetGlobalStat((char*)name, &val);
	return (int)val;
}
DEFINE_PRIM(_I32, get_global_stat, _BYTES);

//-----------------------------------------------------------------------------------------------------------

HL_PRIM vbyte *HL_NAME(get_persona_name)(){
	if(!CheckInit()) return (vbyte*)"Unknown";
	return (vbyte*)SteamFriends()->GetPersonaName();
}
DEFINE_PRIM(_BYTES, get_persona_name, _NO_ARG);

HL_PRIM vbyte *HL_NAME(get_steam_id)(){
	if(!CheckInit()) return (vbyte*)"0";

	CSteamID userId = SteamUser()->GetSteamID();

	std::ostringstream returnData;
	returnData << userId.ConvertToUint64();

	return (vbyte*)returnData.str().c_str();
}
DEFINE_PRIM(_BYTES, get_steam_id, _NO_ARG);

HL_PRIM bool HL_NAME(restart_app_if_necessary)(int appId){
	return SteamAPI_RestartAppIfNecessary(appId);
}
DEFINE_PRIM(_BOOL, restart_app_if_necessary, _I32);

HL_PRIM bool HL_NAME(is_overlay_enabled)(){
	return SteamUtils()->IsOverlayEnabled();
}
DEFINE_PRIM(_BOOL, is_overlay_enabled, _NO_ARG);

HL_PRIM bool HL_NAME(boverlay_needs_present)(){
	return SteamUtils()->BOverlayNeedsPresent();
}
DEFINE_PRIM(_BOOL, boverlay_needs_present, _NO_ARG);

HL_PRIM bool HL_NAME(is_steam_in_big_picture_mode)(){
	return SteamUtils()->IsSteamInBigPictureMode();
}
DEFINE_PRIM(_BOOL, is_steam_in_big_picture_mode, _NO_ARG);

HL_PRIM bool HL_NAME(is_steam_running)(){
	return SteamAPI_IsSteamRunning();
}
DEFINE_PRIM(_BOOL, is_steam_running, _NO_ARG);

HL_PRIM vbyte *HL_NAME(get_current_game_language)(){
	return (vbyte*)SteamApps()->GetCurrentGameLanguage();
}
DEFINE_PRIM(_BYTES, get_current_game_language, _NO_ARG);

//-----------------------------------------------------------------------------------------------------------

//NEW STEAM WORKSHOP---------------------------------------------------------------------------------------------

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

//OLD STEAM WORKSHOP---------------------------------------------------------------------------------------------

HL_PRIM void HL_NAME(get_ugc_download_progress)(vbyte *contentHandle, int *downloaded, int *expected){
	if (!CheckInit()) return;

	uint64 u64Handle = strtoll((char*)contentHandle, NULL, 10);

	SteamRemoteStorage()->GetUGCDownloadProgress(u64Handle, downloaded, expected);
}
DEFINE_PRIM(_VOID, get_ugc_download_progress, _BYTES _REF(_I32) _REF(_I32));

HL_PRIM void HL_NAME(enumerate_user_shared_workshop_files)(vbyte *steamIDStr, int startIndex, vbyte *requiredTagsStr, vbyte *excludedTagsStr){
	if(!CheckInit()) return;

	//Reconstruct the steamID from the string representation
	uint64 u64SteamID = strtoll((char*)steamIDStr, NULL, 10);
	CSteamID steamID = u64SteamID;

	//Construct the string arrays from the comma-delimited strings
	SteamParamStringArray_t * requiredTags = getSteamParamStringArray((char*)requiredTagsStr);
	SteamParamStringArray_t * excludedTags = getSteamParamStringArray((char*)excludedTagsStr);

	//make the actual call
	s_callbackHandler->EnumerateUserSharedWorkshopFiles(steamID, startIndex, requiredTags, excludedTags);

	//clean up requiredTags & excludedTags:
	deleteSteamParamStringArray(requiredTags);
	deleteSteamParamStringArray(excludedTags);
}
DEFINE_PRIM(_VOID, enumerate_user_shared_workshop_files, _BYTES _I32 _BYTES _BYTES);

HL_PRIM void HL_NAME(enumerate_user_published_files)(int startIndex){
	if(!CheckInit()) return;
	uint32 unStartIndex = (uint32) startIndex;
	s_callbackHandler->EnumerateUserPublishedFiles(unStartIndex);
}
DEFINE_PRIM(_VOID, enumerate_user_published_files, _I32);

HL_PRIM void HL_NAME(enumerate_user_subscribed_files)(int startIndex){
	if(!CheckInit()) return;
	uint32 unStartIndex = (uint32) startIndex;
	s_callbackHandler->EnumerateUserSubscribedFiles(unStartIndex);
}
DEFINE_PRIM(_VOID, enumerate_user_subscribed_files, _I32);

HL_PRIM void HL_NAME(get_published_file_details)(vbyte *fileId, int maxSecondsOld){
	if(!CheckInit()) return;

	uint64 u64FileID = strtoull((char*)fileId, NULL, 0);
	uint32 u32MaxSecondsOld = maxSecondsOld;

	s_callbackHandler->GetPublishedFileDetails(u64FileID, u32MaxSecondsOld);
}
DEFINE_PRIM(_VOID, get_published_file_details, _BYTES _I32);

HL_PRIM void HL_NAME(ugc_download)(vbyte *handle, int priority){
	if(!CheckInit()) return;
	uint64 u64Handle = strtoull((char*)handle, NULL, 0);
	uint32 u32Priority = (uint32) priority;
	s_callbackHandler->UGCDownload(u64Handle, u32Priority);
}
DEFINE_PRIM(_VOID, ugc_download, _BYTES _I32);

HL_PRIM int HL_NAME(ugc_read)(vbyte *handle, vbyte *data, int bytesToRead, int offset, int readAction){
	if (!CheckInit()) return 0;

	uint64 u64Handle = strtoull((char*)handle, NULL, 0);

	if(u64Handle == 0 || bytesToRead == 0) return 0;
	return SteamRemoteStorage()->UGCRead(u64Handle, data, bytesToRead, offset, (EUGCReadAction)readAction);
}
DEFINE_PRIM(_I32, ugc_read, _BYTES _BYTES _I32 _I32 _I32);


//STEAM CLOUD------------------------------------------------------------------------------------------------

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


//STEAM CONTROLLER-------------------------------------------------------------------------------------------

HL_PRIM bool HL_NAME(init_controllers)(){
	if (!SteamController()) return false;

	bool result = SteamController()->Init();
	if( result )
		mapControllers.init();
	return result;
}
DEFINE_PRIM(_BOOL, init_controllers, _NO_ARG);

HL_PRIM bool HL_NAME(shutdown_controllers)(){
	bool result = SteamController()->Shutdown();
	if (result)
		mapControllers.init();
	return result;
}
DEFINE_PRIM(_BOOL, shutdown_controllers, _NO_ARG);

HL_PRIM bool HL_NAME(show_binding_panel)(int controller){
	ControllerHandle_t c_handle = controller != -1 ? mapControllers.get(controller) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;

	return SteamController()->ShowBindingPanel(c_handle);
}
DEFINE_PRIM(_BOOL, show_binding_panel, _I32);

HL_PRIM bool HL_NAME(show_gamepad_text_input)(int inputMode, int lineMode, vbyte *description, int charMax, vbyte *existingText){

	EGamepadTextInputMode eInputMode = static_cast<EGamepadTextInputMode>(inputMode);
	EGamepadTextInputLineMode eLineInputMode = static_cast<EGamepadTextInputLineMode>(lineMode);

	return SteamUtils()->ShowGamepadTextInput(eInputMode, eLineInputMode, (char*)description, (uint32)charMax, (char*)existingText);
}
DEFINE_PRIM(_BOOL, show_gamepad_text_input, _I32 _I32 _BYTES _I32 _BYTES);

HL_PRIM vbyte *HL_NAME(get_entered_gamepad_text_input)(){
	uint32 length = SteamUtils()->GetEnteredGamepadTextLength();
	char *pchText = (char *)hl_gc_alloc_noptr(length);
	if( SteamUtils()->GetEnteredGamepadTextInput(pchText, length) )
		return (vbyte*)pchText;
	return (vbyte*)"";

}
DEFINE_PRIM(_BYTES, get_entered_gamepad_text_input, _NO_ARG);

HL_PRIM varray *HL_NAME(get_connected_controllers)(varray *arr){
	SteamController()->RunFrame();

	ControllerHandle_t handles[STEAM_CONTROLLER_MAX_COUNT];
	int result = SteamController()->GetConnectedControllers(handles);

	if( !arr )
		arr = hl_alloc_array(&hlt_i32, STEAM_CONTROLLER_MAX_COUNT);
	int *cur = hl_aptr(arr, int);
	for(int i = 0; i < result; i++)	{
		int index = mapControllers.find(handles[i]);
		if( index < 0 )
			index = mapControllers.add(handles[i]);
		cur[i] = index;
	}

	for (int i = result; i < STEAM_CONTROLLER_MAX_COUNT; i++)
		cur[i] = -1;

	return arr;
}
DEFINE_PRIM(_ARR, get_connected_controllers, _ARR);

HL_PRIM int HL_NAME(get_action_set_handle)(vbyte *actionSetName){
	return (int)SteamController()->GetActionSetHandle((char*)actionSetName);
}
DEFINE_PRIM(_I32, get_action_set_handle, _BYTES);

HL_PRIM int HL_NAME(get_digital_action_handle)(vbyte *actionName){
	return (int)SteamController()->GetDigitalActionHandle((char*)actionName);
}
DEFINE_PRIM(_I32, get_digital_action_handle, _BYTES);

HL_PRIM int HL_NAME(get_analog_action_handle)(vbyte *actionName){
	return (int)SteamController()->GetAnalogActionHandle((char*)actionName);
}
DEFINE_PRIM(_I32, get_analog_action_handle, _BYTES);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM int HL_NAME(get_digital_action_data)(int controllerHandle, int actionHandle){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ControllerDigitalActionHandle_t a_handle = actionHandle;

	ControllerDigitalActionData_t data = SteamController()->GetDigitalActionData(c_handle, a_handle);

	int result = 0;

	//Take both bools and pack them into an int

	if(data.bState)
		result |= 0x1;

	if(data.bActive)
		result |= 0x10;

	return result;
}
DEFINE_PRIM(_I32, get_digital_action_data, _I32 _I32);


//-----------------------------------------------------------------------------------------------------------

typedef struct {
	hl_type *t;
	bool bActive;
	int eMode;
	double x;
	double y;
} analog_action_data;

HL_PRIM void HL_NAME(get_analog_action_data)(int controllerHandle, int actionHandle, analog_action_data *data){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ControllerAnalogActionHandle_t a_handle = actionHandle;

	ControllerAnalogActionData_t d = SteamController()->GetAnalogActionData(c_handle, a_handle);

	data->bActive = d.bActive;
	data->eMode = d.eMode;
	data->x = d.x;
	data->y = d.y;
}
DEFINE_PRIM(_VOID, get_analog_action_data, _I32 _I32 _OBJ(_BOOL _I32 _F64 _F64));

HL_PRIM varray *HL_NAME(get_digital_action_origins)(int controllerHandle, int actionSetHandle, int digitalActionHandle){
	ControllerHandle_t c_handle              = mapControllers.get(controllerHandle);
	ControllerActionSetHandle_t s_handle     = actionSetHandle;
	ControllerDigitalActionHandle_t a_handle = digitalActionHandle;

	EControllerActionOrigin origins[STEAM_CONTROLLER_MAX_ORIGINS];
	for(int i = 0; i < STEAM_CONTROLLER_MAX_ORIGINS; i++)
		origins[i] = k_EControllerActionOrigin_None;

	int result = SteamController()->GetDigitalActionOrigins(c_handle, s_handle, a_handle, origins);
	varray *arr = hl_alloc_array(&hlt_i32, result);
	int *cur = hl_aptr(arr, int);
	for(int i = 0; i < STEAM_CONTROLLER_MAX_ORIGINS; i++)
		cur[i] = origins[i];
	return arr;
}
DEFINE_PRIM(_ARR, get_digital_action_origins, _I32 _I32 _I32);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM varray *HL_NAME(get_analog_action_origins)(int controllerHandle, int actionSetHandle, int analogActionHandle){
	ControllerHandle_t c_handle              = mapControllers.get(controllerHandle);
	ControllerActionSetHandle_t s_handle     = actionSetHandle;
	ControllerAnalogActionHandle_t a_handle  = analogActionHandle;

	EControllerActionOrigin origins[STEAM_CONTROLLER_MAX_ORIGINS];

	//Initialize the whole thing to None to avoid garbage
	for(int i = 0; i < STEAM_CONTROLLER_MAX_ORIGINS; i++)
		origins[i] = k_EControllerActionOrigin_None;

	int result = SteamController()->GetAnalogActionOrigins(c_handle, s_handle, a_handle, origins);
	varray *arr = hl_alloc_array(&hlt_i32, result);
	int *cur = hl_aptr(arr, int);
	for(int i = 0; i < STEAM_CONTROLLER_MAX_ORIGINS; i++)
		cur[i] = origins[i];
	return arr;
}
DEFINE_PRIM(_ARR, get_analog_action_origins, _I32 _I32 _I32);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM vbyte *HL_NAME(get_glyph_for_action_origin)(int origin){
	if (!CheckInit()) return NULL;

	if (origin >= k_EControllerActionOrigin_Count)
		return NULL;

	EControllerActionOrigin eOrigin = static_cast<EControllerActionOrigin>(origin);

	const char * result = SteamController()->GetGlyphForActionOrigin(eOrigin);
	return (vbyte*)result;
}
DEFINE_PRIM(_BYTES, get_glyph_for_action_origin, _I32);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM vbyte *HL_NAME(get_string_for_action_origin)(int origin){
	if (!CheckInit()) return NULL;

	if (origin >= k_EControllerActionOrigin_Count)
		return NULL;

	EControllerActionOrigin eOrigin = static_cast<EControllerActionOrigin>(origin);

	const char * result = SteamController()->GetStringForActionOrigin(eOrigin);
	return (vbyte*)result;
}
DEFINE_PRIM(_BYTES, get_string_for_action_origin, _I32);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM void HL_NAME(activate_action_set)(int controllerHandle, int actionSetHandle){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ControllerActionSetHandle_t a_handle = actionSetHandle;

	SteamController()->ActivateActionSet(c_handle, a_handle);
}
DEFINE_PRIM(_VOID, activate_action_set, _I32 _I32);

//-----------------------------------------------------------------------------------------------------------
HL_PRIM int HL_NAME(get_current_action_set)(int controllerHandle){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	return (int)SteamController()->GetCurrentActionSet(c_handle);
}
DEFINE_PRIM(_I32, get_current_action_set, _I32);

HL_PRIM void HL_NAME(trigger_haptic_pulse)(int controllerHandle, int targetPad, int durationMicroSec){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ESteamControllerPad eTargetPad;
	switch(targetPad)	{
		case 0:  eTargetPad = k_ESteamControllerPad_Left;
		case 1:  eTargetPad = k_ESteamControllerPad_Right;
		default: eTargetPad = k_ESteamControllerPad_Left;
	}
	unsigned short usDurationMicroSec = durationMicroSec;

	SteamController()->TriggerHapticPulse(c_handle, eTargetPad, usDurationMicroSec);
}
DEFINE_PRIM(_VOID, trigger_haptic_pulse, _I32 _I32 _I32);

HL_PRIM void HL_NAME(trigger_repeated_haptic_pulse)(int controllerHandle, int targetPad, int durationMicroSec, int offMicroSec, int repeat, int flags){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ESteamControllerPad eTargetPad;
	switch(targetPad)	{
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
DEFINE_PRIM(_VOID, trigger_repeated_haptic_pulse, _I32 _I32 _I32 _I32 _I32 _I32);

HL_PRIM void HL_NAME(trigger_vibration)(int controllerHandle, int leftSpeed, int rightSpeed){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	SteamController()->TriggerVibration(c_handle, (unsigned short)leftSpeed, (unsigned short)rightSpeed);
}
DEFINE_PRIM(_VOID, trigger_vibration, _I32 _I32 _I32);

HL_PRIM void HL_NAME(set_led_color)(int controllerHandle, int r, int g, int b, int flags){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	SteamController()->SetLEDColor(c_handle, (uint8)r, (uint8)g, (uint8)b, (unsigned int) flags);
}
DEFINE_PRIM(_VOID, set_led_color, _I32 _I32 _I32 _I32 _I32);

//-----------------------------------------------------------------------------------------------------------
typedef struct {
	hl_type *t;
	double rotQuatX;
	double rotQuatY;
	double rotQuatZ;
	double rotQuatW;
	double posAccelX;
	double posAccelY;
	double posAccelZ;
	double rotVelX;
	double rotVelY;
	double rotVelZ;
} motion_data;

HL_PRIM void HL_NAME(get_motion_data)(int controllerHandle, motion_data *data){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	ControllerMotionData_t d = SteamController()->GetMotionData(c_handle);

	data->rotQuatX = d.rotQuatX;
	data->rotQuatY = d.rotQuatY;
	data->rotQuatZ = d.rotQuatZ;
	data->rotQuatW = d.rotQuatW;
	data->posAccelX = d.posAccelX;
	data->posAccelY = d.posAccelY;
	data->posAccelZ = d.posAccelZ;
	data->rotVelX = d.rotVelX;
	data->rotVelY = d.rotVelY;
	data->rotVelZ = d.rotVelZ;
}
DEFINE_PRIM(_VOID, get_motion_data, _I32 _OBJ(_F64 _F64 _F64 _F64 _F64 _F64 _F64 _F64 _F64 _F64));

HL_PRIM bool HL_NAME(show_digital_action_origins)(int controllerHandle, int digitalActionHandle, double scale, double xPosition, double yPosition){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	return SteamController()->ShowDigitalActionOrigins(c_handle, digitalActionHandle, (float)scale, (float)xPosition, (float)yPosition);
}
DEFINE_PRIM(_BOOL, show_digital_action_origins, _I32 _I32 _F64 _F64 _F64);

HL_PRIM bool HL_NAME(show_analog_action_origins)(int controllerHandle, int analogActionHandle, double scale, double xPosition, double yPosition){
	ControllerHandle_t c_handle = controllerHandle != -1 ? mapControllers.get(controllerHandle) : STEAM_CONTROLLER_HANDLE_ALL_CONTROLLERS;
	return SteamController()->ShowAnalogActionOrigins(c_handle, analogActionHandle, (float)scale, (float)xPosition, (float)yPosition);
}
DEFINE_PRIM(_BOOL, show_analog_action_origins, _I32 _I32 _F64 _F64 _F64);


//-----------------------------------------------------------------------------------------------------------
HL_PRIM int HL_NAME(get_controller_max_count)(){
	return STEAM_CONTROLLER_MAX_COUNT;
}
DEFINE_PRIM(_I32, get_controller_max_count, _NO_ARG);

HL_PRIM int HL_NAME(get_controller_max_analog_actions)(){
	return STEAM_CONTROLLER_MAX_ANALOG_ACTIONS;
}
DEFINE_PRIM(_I32, get_controller_max_analog_actions, _NO_ARG);

HL_PRIM int HL_NAME(get_controller_max_digital_actions)(){
	return STEAM_CONTROLLER_MAX_DIGITAL_ACTIONS;
}
DEFINE_PRIM(_I32, get_controller_max_digital_actions, _NO_ARG);

HL_PRIM int HL_NAME(get_controller_max_origins)(){
	return STEAM_CONTROLLER_MAX_ORIGINS;
}
DEFINE_PRIM(_I32, get_controller_max_origins, _NO_ARG);

HL_PRIM double HL_NAME(get_controller_min_analog_action_data)(){
	return STEAM_CONTROLLER_MIN_ANALOG_ACTION_DATA;
}
DEFINE_PRIM(_F64, get_controller_min_analog_action_data, _NO_ARG);

HL_PRIM double HL_NAME(get_controller_max_analog_action_data)(){
	return STEAM_CONTROLLER_MAX_ANALOG_ACTION_DATA;
}
DEFINE_PRIM(_F64, get_controller_max_analog_action_data, _NO_ARG);


// -----------------------
// LOBBY

HL_PRIM void HL_NAME(cancel_call_result)( CClosureCallResult<int> *m_call ) {
	m_call->Cancel();
	delete m_call;
}

DEFINE_PRIM(_VOID, cancel_call_result, _CRESULT);

static void dyn_call_result( vclosure *c, vdynamic *p, bool error ) {
	vdynamic b;
	vdynamic *args[2];
	args[0] = p;
	args[1] = &b;
	b.t = &hlt_bool;
	b.v.b = error;
	hl_dyn_call(c,args,2);
}

static void on_lobby_list( vclosure *c, LobbyMatchList_t *result, bool error ) {
	vdynamic d;
	d.t = &hlt_i32;
	d.v.i = error ? -1 : result->m_nLobbiesMatching;
	dyn_call_result(c,&d,error);
}

HL_PRIM CClosureCallResult<LobbyMatchList_t>* HL_NAME(request_lobby_list)( vclosure *closure ) {
	if( !CheckInit() ) return NULL;
	ASYNC_CALL(SteamMatchmaking()->RequestLobbyList(), LobbyMatchList_t, on_lobby_list);
	return m_call;
}
DEFINE_PRIM(_CRESULT, request_lobby_list, _CALLB(_I32));

} // extern "C"

