@rem ===========================================================================
@rem SCAFFOLDING - NOT RECOVERED SOURCE. NOT PART OF THE BF2 BUILD.
@rem
@rem Runtime verification for the CBF2VoipServer facade (Phase 6). Binds loopback ports
@rem 27185/27186 (never 16567).
@rem ===========================================================================
@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86
pushd "%~dp0"
set SRC=%~dp0..\BF2
cl /nologo /EHsc /O2 /DDEDICATED=1 /FI"%SRC%\Core\BuildConfig.h" ^
   voip_facade_runtime.cpp ^
   "%SRC%\VoipServer\VoipServer.cpp" ^
   "%SRC%\VoipServer\VoipServerInstance.cpp" ^
   "%SRC%\VoipServer\VoipDebug.cpp" ^
   "%SRC%\VoipServer\VoipRouter.cpp" ^
   "%SRC%\VoipServer\VoipClients.cpp" ^
   "%SRC%\VoipServer\VoipOutgoingChannels.cpp" ^
   "%SRC%\Core\Timer.cpp" ^
   "%SRC%\Core\Mutex.cpp" ^
   "%SRC%\Core\SystemInfo.cpp" ^
   "%SRC%\Core\Debug.cpp" ^
   "%SRC%\Core\StringUtil.cpp" ^
   "%SRC%\IO\Network\NetworkLayer\Socket\UDPSocket.cpp" ^
   "%SRC%\IO\Network\SocketManager.cpp" ^
   "%SRC%\External\GameSpy\md5c.c" ^
   /link ws2_32.lib
if errorlevel 1 ( popd & exit /b 1 )
"%~dp0voip_facade_runtime.exe"
set RC=%errorlevel%
popd
exit /b %RC%
