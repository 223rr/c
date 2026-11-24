@echo off
setlocal
set "GPP=C:\Users\is\Desktop\p3\mingw64\mingw64\bin\g++.exe"
set "ROOT=%~dp0"
set "FLAGS=-std=c++17 -Wall -Wextra -g -static"

if exist "%GPP%" goto have_gpp
echo [build] cannot find compiler: %GPP%
exit /b 1

:have_gpp
if not exist "%ROOT%build" mkdir "%ROOT%build"
if /I "%~1"=="cli" goto build_cli
if /I "%~1"=="server" goto build_server
if /I "%~1"=="serve" goto build_and_serve
if /I "%~1"=="tests" goto build_tests
if /I "%~1"=="test" goto build_and_run
echo Usage: build.bat cli ^| server ^| serve ^| tests ^| test
exit /b 2

:build_cli
"%GPP%" %FLAGS% "%ROOT%main.cpp" "%ROOT%task.cpp" -o "%ROOT%build\task_cli.exe"
exit /b %ERRORLEVEL%

:build_server
"%GPP%" %FLAGS% "%ROOT%server.cpp" "%ROOT%task.cpp" -o "%ROOT%build\task_server.exe" -lws2_32 -lshell32
exit /b %ERRORLEVEL%

:build_and_serve
"%GPP%" %FLAGS% "%ROOT%server.cpp" "%ROOT%task.cpp" -o "%ROOT%build\task_server.exe" -lws2_32 -lshell32
if errorlevel 1 exit /b 1
call "%ROOT%build\task_server.exe" 8080
exit /b %ERRORLEVEL%

:build_tests
"%GPP%" %FLAGS% "%ROOT%test.cpp" "%ROOT%task.cpp" -o "%ROOT%build\task_tests.exe"
exit /b %ERRORLEVEL%

:build_and_run
"%GPP%" %FLAGS% "%ROOT%test.cpp" "%ROOT%task.cpp" -o "%ROOT%build\task_tests.exe"
if errorlevel 1 exit /b 1
call "%ROOT%build\task_tests.exe"
exit /b %ERRORLEVEL%
