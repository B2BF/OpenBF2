@rem ===========================================================================
@rem SCAFFOLDING - NOT RECOVERED SOURCE. NOT PART OF THE BF2 BUILD.
@rem
@rem Runtime verification for VOIP::CVoipRouterInstance (Phase 2). Drives the
@rem packet pump and voice-frame fan-out behind a mock transport.
@rem ===========================================================================
@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86
pushd "%~dp0"
cl /nologo /EHsc /O2 /DDEDICATED=1 /FI"%~dp0..\BF2\Core\BuildConfig.h" ^
   voip_router_runtime.cpp ^
   "%~dp0..\BF2\VoipServer\VoipRouter.cpp" ^
   "%~dp0..\BF2\VoipServer\VoipClients.cpp" ^
   "%~dp0..\BF2\VoipServer\VoipOutgoingChannels.cpp"
if errorlevel 1 ( popd & exit /b 1 )
"%~dp0voip_router_runtime.exe"
set RC=%errorlevel%
popd
exit /b %RC%
