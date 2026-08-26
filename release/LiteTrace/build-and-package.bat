@echo off
setlocal enabledelayedexpansion
rem One-click LiteTrace release: configure + build the LiteTrace target, then
rem stage LiteTrace.dll + LiteTrace.ini + README into dist\LiteTrace-v<version>\.
rem Run from anywhere; paths are resolved relative to this script.

set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%..\.."
set "VERSION=0.1.0"
set "BUILD_DIR=%REPO_ROOT%\obj\cmake-x64-release"

rem Locate Visual Studio 2022 via vswhere (falls back to the common path).
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSPATH="
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -property installationPath`) do set "VSPATH=%%i"
)
if not defined VSPATH set "VSPATH=D:\Program Files\Microsoft Visual Studio\2022\Community"

if not exist "%VSPATH%\Common7\Tools\VsDevCmd.bat" (
    echo [error] Could not find VsDevCmd.bat. Edit VSPATH in this script to your VS2022 install path.
    exit /b 1
)

call "%VSPATH%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%

cmake -S "%REPO_ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b %errorlevel%

cmake --build "%BUILD_DIR%" --config Release --target LiteTrace --parallel
if errorlevel 1 exit /b %errorlevel%

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%package.ps1" -Version %VERSION%
if errorlevel 1 exit /b %errorlevel%

endlocal
