@echo off
setlocal enabledelayedexpansion

echo ========================================================
echo  Pak Pharmacy POS - Standalone Release Bundler
echo ========================================================

set "PROJECT_ROOT=%~dp0.."
cd /d "%PROJECT_ROOT%"

set "UCRT64_BIN=C:\msys64\ucrt64\bin"
set "PATH=%UCRT64_BIN%;C:\Windows\system32;C:\Windows;%PATH%"

set "DIST_DIR=%PROJECT_ROOT%\dist\PakPharmacyPOS"

echo [1/5] Building Release Executable...
taskkill /F /IM PakPharmacyPOS.exe 2>nul
cmake --build build --target PakPharmacyPOS
if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo [2/5] Creating Clean Dist Directory...
if exist "%DIST_DIR%" (
    rmdir /s /q "%DIST_DIR%"
)
mkdir "%DIST_DIR%"

echo [3/5] Copying Application Binary...
copy /y "%PROJECT_ROOT%\build\PakPharmacyPOS.exe" "%DIST_DIR%\"
if not exist "%DIST_DIR%\PakPharmacyPOS.exe" (
    echo [ERROR] PakPharmacyPOS.exe not found in build directory!
    exit /b 1
)

echo [4/5] Deploying Qt6 Runtimes and Plugins (windeployqt)...
windeployqt --release --no-translations --compiler-runtime --dir "%DIST_DIR%" "%DIST_DIR%\PakPharmacyPOS.exe"

echo [5/5] Copying MinGW UCRT64 System Runtimes...
for %%F in (
    libwinpthread-1.dll
    libgcc_s_seh-1.dll
    libstdc++-6.dll
    libsqlite3-0.dll
    zlib1.dll
) do (
    if exist "%UCRT64_BIN%\%%F" (
        copy /y "%UCRT64_BIN%\%%F" "%DIST_DIR%\" >nul
        echo   + Copied %%F
    )
)

:: Ensure sqldrivers plugin exists
if not exist "%DIST_DIR%\sqldrivers" mkdir "%DIST_DIR%\sqldrivers"
if exist "C:\msys64\ucrt64\share\qt6\plugins\sqldrivers\qsqlite.dll" (
    copy /y "C:\msys64\ucrt64\share\qt6\plugins\sqldrivers\qsqlite.dll" "%DIST_DIR%\sqldrivers\" >nul
    echo   + Copied sqldrivers\qsqlite.dll
)

:: Create quick launch batch file
(
echo @echo off
echo cd /d "%%~dp0"
echo start "" "%%~dp0PakPharmacyPOS.exe" %%*
) > "%DIST_DIR%\Launch_POS.bat"

echo.
echo ========================================================
echo  SUCCESS: Standalone release package ready at:
echo  %DIST_DIR%
echo ========================================================
endlocal
