@echo off
call %1

if not exist "%DIR_BIN%" mkdir "%DIR_BIN%"
copy /V /Y "%BUILD_OUT_DIR%\%BUILD_OUT_BASENAME%" /B "%DIR_BIN%\" >NUL
