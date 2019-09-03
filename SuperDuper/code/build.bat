@echo off

SET CompilerFlags=/nologo /Z7 /Od /Oi /MTd /fp:fast /FC /WX /W4 /wd4201 /wd4100 /wd4189 /wd4456 /wd4505 /wd4324 /DSUPER_INTERNAL=1
SET LinkerFlags=/incremental:no /opt:ref user32.lib Comctl32.lib gdi32.lib shell32.lib Comdlg32.lib

REM SET CompilerFlags=/nologo /Z7 /Ox /Oi /MT /fp:fast /FC /WX /W4 /wd4201 /wd4100 /wd4189 /wd4456 /wd4505 /wd4324 /DSUPER_INTERNAL=1

IF NOT EXIST build mkdir build
pushd build

REM rc.exe /nologo ..\SuperDuper\code\resources.rc
cl.exe %CompilerFlags% ..\SuperDuper\code\resources.res ..\SuperDuper\code\win32_superduper.cpp /link %LinkerFlags%

popd
