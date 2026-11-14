@rem ===========================================================================
@rem SCAFFOLDING - NOT RECOVERED SOURCE. NOT PART OF THE BF2 BUILD.
@rem
@rem Runtime verification for the voice-server debug logs (frontier closure batch 3):
@rem HandlePacketDebugRoutingData, the routing dump and the voice trace, read back from the files
@rem they write in a scratch directory. No sockets are bound. macOS twin: CTest bf2_voip_debug_check.
@rem ===========================================================================
@echo off
setlocal EnableDelayedExpansion
call "%~dp0..\Build\vsdevcmd.cmd" x86
pushd "%~dp0"
set SRC=%~dp0..\BF2
cl /nologo /EHsc /O2 /DDEDICATED=1 /FI"%SRC%\Core\BuildConfig.h" ^
   voip_debug_runtime.cpp ^
   "%SRC%\VoipServer\VoipServerInstance.cpp" ^
   "%SRC%\VoipServer\VoipDebug.cpp" ^
   "%SRC%\VoipServer\VoipRouter.cpp" ^
   "%SRC%\VoipServer\VoipClients.cpp" ^
   "%SRC%\VoipServer\VoipOutgoingChannels.cpp" ^
   "%SRC%\Core\Timer.cpp" ^
   "%SRC%\Core\SystemInfo.cpp" ^
   "%SRC%\Core\Debug.cpp" ^
   "%SRC%\Core\StringUtil.cpp" ^
   "%SRC%\IO\Network\NetworkLayer\Socket\UDPSocket.cpp" ^
   "%SRC%\External\GameSpy\md5c.c" ^
   /link ws2_32.lib > voip_debug_runtime.link.log 2>&1
if errorlevel 1 (
  type voip_debug_runtime.link.log
  popd & exit /b 1
)
"%~dp0voip_debug_runtime.exe"
set "RC=!errorlevel!"
echo voip_debug_runtime.exe exit !RC!
popd
exit /b !RC!
