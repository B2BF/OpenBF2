@rem ===========================================================================
@rem SCAFFOLDING - NOT RECOVERED SOURCE. NOT PART OF THE BF2 BUILD.
@rem
@rem Runtime verification for CBF2VoipServerInstance state and getters (Phase 3). The UDP
@rem transport, challenge handshake and BF2 packet handlers are named frontiers here.
@rem ===========================================================================
@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86
pushd "%~dp0"
cl /nologo /EHsc /O2 /DDEDICATED=1 /FI"%~dp0..\BF2\Core\BuildConfig.h" ^
   voip_instance_runtime.cpp ^
   "%~dp0..\BF2\VoipServer\VoipServerInstance.cpp" ^
   "%~dp0..\BF2\VoipServer\VoipDebug.cpp" ^
   "%~dp0..\BF2\VoipServer\VoipRouter.cpp" ^
   "%~dp0..\BF2\VoipServer\VoipClients.cpp" ^
   "%~dp0..\BF2\VoipServer\VoipOutgoingChannels.cpp" ^
   "%~dp0..\BF2\Core\Timer.cpp" ^
   "%~dp0..\BF2\Core\SystemInfo.cpp" ^
   "%~dp0..\BF2\Core\Debug.cpp" ^
   "%~dp0..\BF2\Core\StringUtil.cpp" ^
   "%~dp0..\BF2\IO\Network\NetworkLayer\Socket\UDPSocket.cpp" ^
   "%~dp0..\BF2\External\GameSpy\md5c.c" ^
   /link ws2_32.lib
if errorlevel 1 ( popd & exit /b 1 )
"%~dp0voip_instance_runtime.exe"
set RC=%errorlevel%
popd
exit /b %RC%
