@echo off
setlocal
cd /d "%~dp0"
set "PREMAKE=premake5.exe"
if exist "C:\dev\tools\premake5.exe" set "PREMAKE=C:\dev\tools\premake5.exe"
if exist "tools\premake5.exe" set "PREMAKE=%~dp0tools\premake5.exe"
if not "%~1"=="" set "PREMAKE=%~1"
"%PREMAKE%" vs2022
exit /b %errorlevel%
