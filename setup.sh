#!/bin/bash
function pause(){
   read -p "$*"
}

echo "Enter FULL path (no ~, etc) to Steam SDK root (folder that contains 'redistributable_bin', no trailing slash):"
read STEAMPATH
echo "Enter FULL path (no ~, etc) to hashlink root:"
read HASHLINK

cp "$STEAMPATH"/public/steam/*.h native/include/steam
cp "$STEAMPATH"/redistributable_bin/steam_api.dll native/lib/win32 > /dev/null
cp "$STEAMPATH"/redistributable_bin/steam_api.lib native/lib/win32 > /dev/null
cp "$STEAMPATH"/redistributable_bin/osx32/libsteam_api.dylib native/lib/osx64 > /dev/null
cp "$STEAMPATH"/redistributable_bin/linux32/libsteam_api.so native/lib/linux32 > /dev/null
cp "$STEAMPATH"/redistributable_bin/linux64/libsteam_api.so native/lib/linux64 > /dev/null
cp "$HASHLINK"/src/hl.h native/include > /dev/null
