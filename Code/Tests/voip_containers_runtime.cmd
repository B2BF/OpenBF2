@rem ===========================================================================
@rem SCAFFOLDING - NOT RECOVERED SOURCE. NOT PART OF THE BF2 BUILD.
@rem
@rem Runtime verification for the recovered VOIP container library (Phase 1):
@rem   VOIP::ClientData / CClients / COutgoingChannels / SessionIdArray / ChannelArray
@rem Compiles the two recovered translation units and the check, then runs it.
@rem ===========================================================================
@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86
pushd "%~dp0"
cl /nologo /EHsc /O2 /DDEDICATED=1 /FI"%~dp0..\BF2\Core\BuildConfig.h" ^
   voip_containers_runtime.cpp ^
   "%~dp0..\BF2\VoipServer\VoipClients.cpp" ^
   "%~dp0..\BF2\VoipServer\VoipOutgoingChannels.cpp"
if errorlevel 1 ( popd & exit /b 1 )
"%~dp0voip_containers_runtime.exe"
set RC=%errorlevel%
popd
exit /b %RC%
