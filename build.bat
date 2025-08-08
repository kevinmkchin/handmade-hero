@echo off

set CommonCompilerFlags=-MT -nologo -WX -W4 -wd4201 -wd4100 -wd4189 -D_CRT_SECURE_NO_WARNINGS -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7
set CommonLinkerFlags=-incremental:no -opt:ref -subsystem:windows user32.lib gdi32.lib winmm.lib

IF NOT EXIST build mkdir build
pushd build
cl %CommonCompilerFlags% /LD ..\code\handmade.cpp -Fmhandmade.map /link /EXPORT:GameUpdateAndRender /EXPORT:GameGetSoundSamples
cl %CommonCompilerFlags% ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd
