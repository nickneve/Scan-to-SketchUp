@echo off
rem Builds housegen and the tests with Visual Studio's bundled CMake + Ninja.
rem   build.cmd          configure and build (Release)
rem   build.cmd test     ...then run the unit tests
rem Set VSDEVCMD to override the Visual Studio developer prompt location.
setlocal
if not defined VSDEVCMD set "VSDEVCMD=C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
  echo Visual Studio developer prompt not found at "%VSDEVCMD%". Set VSDEVCMD.
  exit /b 1
)
call "%VSDEVCMD%" -arch=x64 -no_logo >nul 2>&1
cmake -S "%~dp0." -B "%~dp0build" -G Ninja -DCMAKE_BUILD_TYPE=Release >nul || exit /b 1
cmake --build "%~dp0build" || exit /b 1
if /i "%~1"=="test" "%~dp0build\s2s_tests.exe" || exit /b 1
