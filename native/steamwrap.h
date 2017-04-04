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

typedef vbyte *		vuid;
#define _UID		_BYTES
#define hlt_uid		hlt_bytes

void dyn_call_result( vclosure *c, vdynamic *p, bool error );
void hl_set_uid( vdynamic *out, int64 uid );
CSteamID hl_to_uid( vuid v );

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

#define EVENT_CALLBACK(name,type) \
	STEAM_CALLBACK(Callbacks, _On##name, type, name); \
	void On##name( type *t ) { GlobalEvent(type::k_iCallback, Encode##name(t)); }

#define _CRESULT	_ABSTRACT(steam_call_result)
#define _CALLB(T)	_FUN(_VOID, T _BOOL)

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
	ItemDownloaded,
	ItemInstalled,
	UGCQueryCompleted
} event_type;

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

	void FileShare(const char* fileName);
	void OnFileShared( RemoteStorageFileShareResult_t *pResult, bool bIOFailure);
	CCallResult<CallbackHandler, RemoteStorageFileShareResult_t> m_callResultFileShare;

};

class HLValue {
public:
	vdynamic *value;
	HLValue() {
		value = (vdynamic*)hl_alloc_dynobj();
	}
	void Set( const char *name, int64 uid ) {
		vdynamic *d = hl_alloc_dynamic(&hlt_uid);
		hl_set_uid(d,uid);
		Set(name, d);
	}
	void Set( const char *name, vdynamic *d ) {
		hl_dyn_setp(value, hl_hash_utf8(name), &hlt_dyn, d);
	}
};

extern CallbackHandler *s_callbackHandler;

void GlobalEvent( int type, vdynamic *data );
void SendEvent(event_type type, bool success, const char *data);
bool CheckInit();

SteamParamStringArray_t * getSteamParamStringArray(const char * str);
void deleteSteamParamStringArray(SteamParamStringArray_t * params);
void split(const std::string &s, char delim, std::vector<std::string> &elems);
