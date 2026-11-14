@echo off
rem [TEMP] fourway comparison: one-time guest setup (never writes into C:\Battlefield 2).
rem   C:\rendsrv\            retail dedicated server root: copies of the server executables and DLLs,
rem                          read-only junctions / hard links to the content, real copies of every
rem                          directory the server writes or the teleport module needs (mods\bf2\python,
rem                          mods\bf2\Settings, mods\bf2\Logs, Admin, python, Settings, Profiles)
rem   C:\rendfw\home-template USERPROFILE template: the retail test profile (C:\rendret\home), renamed
rem                          LoopBot like the macOS harness profile; Controls.con / Video.con as given
rem                          in C:\rendfw\prof (copied there by fourway.py)
setlocal
set "G=C:\Battlefield 2"
set "S=C:\rendsrv"
if not exist %S% mkdir %S%
for %%F in ("%G%\*.exe" "%G%\*.dll") do (
  echo %%~nxF | findstr /i "inject redirect Editor _r\." >nul || copy /y "%%F" %S%\ >nul
)
copy /y "%G%\pylib-2.3.4.zip" %S%\ >nul
robocopy "%G%\Admin" %S%\Admin /E /NFL /NDL /NJH /NJS /NP >nul
robocopy "%G%\python" %S%\python /E /NFL /NDL /NJH /NJS /NP >nul
robocopy "%G%\Settings" %S%\Settings /E /NFL /NDL /NJH /NJS /NP >nul
robocopy "%G%\Profiles" %S%\Profiles /E /NFL /NDL /NJH /NJS /NP >nul
if not exist %S%\mods\bf2 mkdir %S%\mods\bf2
for /d %%D in ("%G%\mods\bf2\*") do (
  if /i not "%%~nxD"=="python" if /i not "%%~nxD"=="Logs" if /i not "%%~nxD"=="Settings" (
    if not exist "%S%\mods\bf2\%%~nxD" mklink /J "%S%\mods\bf2\%%~nxD" "%%D" >nul
  )
)
robocopy "%G%\mods\bf2\python" %S%\mods\bf2\python /E /NFL /NDL /NJH /NJS /NP >nul
robocopy "%G%\mods\bf2\Settings" %S%\mods\bf2\Settings /E /NFL /NDL /NJH /NJS /NP >nul
if not exist %S%\mods\bf2\Logs mkdir %S%\mods\bf2\Logs
for %%F in ("%G%\mods\bf2\*.*") do if not exist "%S%\mods\bf2\%%~nxF" mklink /H "%S%\mods\bf2\%%~nxF" "%%F" >nul
rem the profile template
if exist C:\rendfw\home-template rmdir /s /q C:\rendfw\home-template
robocopy C:\rendret\home C:\rendfw\home-template /E /NFL /NDL /NJH /NJS /NP >nul
set "P=C:\rendfw\home-template\Documents\Battlefield 2\Profiles\0001"
copy /y C:\rendfw\prof\Profile.con "%P%\Profile.con" >nul
copy /y C:\rendfw\prof\Controls.con "%P%\Controls.con" >nul
copy /y C:\rendfw\prof\Video.con "%P%\Video.con" >nul
dir /b %S% | find /c /v ""
dir /b %S%\mods\bf2
exit /b 0
