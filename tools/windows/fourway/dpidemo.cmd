@echo off
rem [TEMP] fourway DPI demo: start an exe to the main menu (no +joinServer) at 800x600 windowed WITHOUT
rem the __COMPAT_LAYER variable, so only a registry layer entry (if any) applies. The login screen is
rem passed by C:\rendfw\scripts\login.txt: through the dinput8 proxy (retail) or BF2_INPUT_SCRIPT (ours).
rem usage: dpidemo.cmd EXEDIR ROOTDIR HOMEDIR px|is
set "USERPROFILE=%~3"
if /i "%~4"=="px" (set "DI8PROXY_SCRIPT=C:\rendfw\scripts\login.txt" & set "DI8PROXY_REPORT=%~3\proxy.log")
if /i "%~4"=="is" set "BF2_INPUT_SCRIPT=C:\rendfw\scripts\login.txt"
cd /d "%~2"
"%~1\BF2.exe" +modPath mods/bf2 +menu 1 +fullscreen 0 +szx 800 +szy 600 +restart 1 +playerName LoopBot > "%~3\client.out" 2>&1
