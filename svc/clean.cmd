@echo off
pushd "%~dp0"

if exist _bin rmdir /s /q _bin
if exist _build rmdir /s /q _build

popd
