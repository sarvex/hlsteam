#include "steamwrap.h"

vdynamic *CallbackHandler::EncodePersonaChange( PersonaStateChange_t *d ) {
	HLValue ret;
	ret.Set("user",d->m_ulSteamID);
	ret.Set("flags", d->m_nChangeFlags);
	return ret.value;
}

HL_PRIM vbyte *HL_NAME(get_user_name)( vuid uid ) {
	return (vbyte*)SteamFriends()->GetFriendPersonaName(hl_to_uid(uid));
}

HL_PRIM bool HL_NAME(request_user_information)( vuid uid, bool nameOnly ) {
	return SteamFriends()->RequestUserInformation(hl_to_uid(uid), nameOnly);
}

HL_PRIM vbyte *HL_NAME(get_user_avatar)( vuid uid, int size, uint32 *width, uint32 *height ) {
	int img = 0;
	vbyte *out;
	switch( size ) {
	case 0: img = SteamFriends()->GetSmallFriendAvatar(hl_to_uid(uid)); break;
	case 1: img = SteamFriends()->GetMediumFriendAvatar(hl_to_uid(uid)); break;
	case 2: img = SteamFriends()->GetLargeFriendAvatar(hl_to_uid(uid)); break;
	}
	if( img == 0 )
		return NULL;
	if( !SteamUtils()->GetImageSize(img,width,height) )
		return NULL;
	size = *width * *height * 4;
	out = (vbyte*)hl_gc_alloc_noptr(size);
	if( !SteamUtils()->GetImageRGBA(img,out,size) )
		return NULL;
	return out;
}

DEFINE_PRIM(_BYTES, get_user_name, _UID);
DEFINE_PRIM(_BYTES, get_user_avatar, _UID _I32 _REF(_I32) _REF(_I32));
DEFINE_PRIM(_BOOL, request_user_information, _UID _BOOL);
