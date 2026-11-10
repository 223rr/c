@echo off
setlocal
set "GPP=C:\Users\is\Desktop\p3\mingw64\mingw64\bin\g++.exe"
set "ROOT=%~dp0"

if exist "%GPP%" goto have_gpp
echo [build] cannot find compiler: %GPP%
exit /b 1

:have_gpp
if not exist "%ROOT%build" mkdir "%ROOT%build"
if /I "%~1"=="cli" goto build_cli
if /I "%~1"=="tests" goto build_tests
if /I "%~1"=="test" goto build_and_run
echo Usage: build.bat cli ^| tests ^| test
exit /b 2

:build_cli
"%GPP%" -std=c++17 -Wall -Wextra -g "%ROOT%main.cpp" "%ROOT%task.cpp" -o "%ROOT%build\task_cli.exe"
exit /b %ERRORLEVEL%

:build_tests
"%GPP%" -std=c++17 -Wall -Wextra -g "%ROOT%test.cpp" "%ROOT%task.cpp" -o "%ROOT%build\task_tests.exe"
exit /b %ERRORLEVEL%

:build_and_run
"%GPP%" -std=c++17 -Wall -Wextra -g "%ROOT%test.cpp" "%ROOT%task.cpp" -o "%ROOT%build\task_tests.exe"
if errorlevel 1 exit /b 1
call "%ROOT%build\task_tests.exe"
exit /b %ERRORLEVEL%
