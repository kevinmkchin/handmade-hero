@echo off

set CommonCompilerFlags=-MTd -nologo -WX -W4 -wd4201 -wd4100 -wd4189 -wd4505 -D_CRT_SECURE_NO_WARNINGS -DHANDMADE_INTERNAL=1 -DHANDMADE_SLOW=1 -FC -Z7
set CommonLinkerFlags=-incremental:no -opt:ref -subsystem:windows user32.lib gdi32.lib winmm.lib

set ts=%date:~10,4%%date:~4,2%%date:~7,2%_%time:~0,2%%time:~3,2%
set ts=%ts: =0%%time:~3,2%%time:~6,2%

IF NOT EXIST build mkdir build
pushd build
del *.pdb > NUL 2> NUL
cl %CommonCompilerFlags% -LD ..\code\handmade.cpp -Fmhandmade.map /link -incremental:no -PDB:handmade_%ts%.pdb -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples
cl %CommonCompilerFlags% ..\code\win32_handmade.cpp -Fmwin32_handmade.map /link %CommonLinkerFlags%
popd


rem when PDB name does not match binary name, Superluminal is not able to find debug info

