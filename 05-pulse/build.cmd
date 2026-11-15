@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1
if not exist build mkdir build
if errorlevel 1 goto failed
q++ -Vgcc_ntox86_64 -std=c++17 -D_QNX_SOURCE -g -O0 -Wall -Wextra -o build/server server.cpp
if errorlevel 1 goto failed
q++ -Vgcc_ntox86_64 -std=c++17 -D_QNX_SOURCE -g -O0 -Wall -Wextra -o build/client client.cpp
if errorlevel 1 goto failed
echo [build] 05-pulse server/client built successfully.
popd
exit /b 0
:failed
popd
exit /b 1
