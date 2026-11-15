@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1
if not exist build mkdir build
if errorlevel 1 goto failed
set FLAGS=-Vgcc_ntox86_64 -std=c++17 -D_QNX_SOURCE -g -O0 -Wall -Wextra -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Werror
q++ %FLAGS% -o build/server server.cpp stub.cpp
if errorlevel 1 goto failed
q++ %FLAGS% -o build/client client.cpp proxy.cpp
if errorlevel 1 goto failed
echo [build] 09-fidl-flow PASS
popd
exit /b 0
:failed
popd
exit /b 1
