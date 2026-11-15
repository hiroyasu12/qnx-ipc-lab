@echo off
setlocal
pushd "%~dp0"
if errorlevel 1 exit /b 1
call 09-fidl-flow\build.cmd
if errorlevel 1 goto failed
call 10-posix-mqueue\build.cmd
if errorlevel 1 goto failed
call 11-pipe\build.cmd
if errorlevel 1 goto failed
call 12-fifo\build.cmd
if errorlevel 1 goto failed
echo [build] All learning chapters PASS
popd
exit /b 0
:failed
popd
exit /b 1
