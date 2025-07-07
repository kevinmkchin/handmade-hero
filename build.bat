@echo off

IF NOT EXIST build mkdir build
pushd build
cl -MT -nologo -WX -W4 -wd4201 -wd4100 -wd4189 -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7 -Fmwin32_handmade.map ..\code\win32_handmade.cpp /link -opt:ref -subsystem:windows user32.lib gdi32.lib
popd
