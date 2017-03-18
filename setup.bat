@echo off
set /p STEAMPATH=Enter path to Steam SDK root (folder that contains "\redistributable_bin", no trailing slash):
set /p HASHLINK=Enter path to hashlink root:
copy "%STEAMPATH%\public\steam\*.h" native\include\steam\
copy "%STEAMPATH%\redistributable_bin\steam_api.dll" native\lib\win32\
copy "%STEAMPATH%\redistributable_bin\steam_api.lib" native\lib\win32\
copy "%STEAMPATH%\redistributable_bin\osx32\libsteam_api.dylib" native\lib\osx64
copy "%STEAMPATH%\redistributable_bin\linux32\libsteam_api.so" native\lib\linux32
copy "%STEAMPATH%\redistributable_bin\linux64\libsteam_api.so" native\lib\linux64
copy "%HASHLINK%\src\hl.h" native\include
pause