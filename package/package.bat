@echo off
setlocal

set WIN_BIN_DIR=../x64/Release
set LINUX_BIN_DIR=../build
set FILES_DIR=../otherFiles

set OUT_DIR=out
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

echo Packaging mod.dll...
call :package dll "%WIN_BIN_DIR%" "PracticeMod_Windows.zip"
echo.
echo Packaging mod.so...
call :package so "%LINUX_BIN_DIR%" "PracticeMod_Linux.zip"
goto :end

:package
set "EXT=%~1"
set "PACKAGE_BIN_DIR=%~2"
set "ZIP_NAME=%~3"
set "TEMP_DIR=%OUT_DIR%\temp_%EXT%"

if exist "%TEMP_DIR%" rmdir /s /q "%TEMP_DIR%"
mkdir "%TEMP_DIR%" 2>nul
mkdir "%TEMP_DIR%\PracticeMod\data" 2>nul

if not exist "%PACKAGE_BIN_DIR%\mod.%EXT%" (
    echo Warning: mod.%EXT% not found in "%BIN_DIR%"
    goto :eof
)
if not exist "%FILES_DIR%\mod.yc" (
    echo Warning: mod.yc not found in "%FILES_DIR%"
    goto :eof
)
if not exist "%FILES_DIR%\debugdraw.font.yc" (
    echo Warning: debugdraw.font.yc not found in "%FILES_DIR%"
    goto :eof
)

copy /y "%PACKAGE_BIN_DIR%\mod.%EXT%" "%TEMP_DIR%\PracticeMod\" >nul
copy /y "%FILES_DIR%\mod.yc" "%TEMP_DIR%\PracticeMod\" >nul
copy /y "%FILES_DIR%\debugdraw.font.yc" "%TEMP_DIR%\PracticeMod\data\" >nul

echo Attempting to create zip...

:: Use native PowerShell ZIP (Windows 10+)
powershell -NoProfile -ExecutionPolicy Bypass -Command "Compress-Archive -LiteralPath '%TEMP_DIR%\PracticeMod' -DestinationPath '%OUT_DIR%\%ZIP_NAME%' -Force"
rmdir /s /q "%TEMP_DIR%"

goto :eof

:end
echo Exiting.
