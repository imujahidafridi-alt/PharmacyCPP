@echo off
setlocal
cd /d "%~dp0"
set "PATH=%~dp0build;C:\msys64\ucrt64\bin;%SystemRoot%\system32;%SystemRoot%;%PATH%"
start "" "%~dp0build\PakPharmacyPOS.exe" %*
endlocal
