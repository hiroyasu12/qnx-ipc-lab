@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1
if not exist build mkdir build
if errorlevel 1 goto failed
q++ -Vgcc_ntox86_64 -std=c++17 -D_QNX_SOURCE -g -O0 -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Werror -o build/pipe_demo pipe_demo.cpp
if errorlevel 1 goto failed
echo [build] 11-pipe PASS
popd
exit /b 0
:failed
popd
exit /b 1
