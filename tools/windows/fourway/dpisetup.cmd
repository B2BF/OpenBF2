@echo off
rem [TEMP] fourway DPI demo: stage the rebuilt x86 client in C:\rendbuild\dpidemo (a path with no
rem compatibility layer entry) and two fresh profile copies.
set "D=C:\rendbuild\dpidemo"
if not exist %D% mkdir %D%
robocopy C:\rendx86 %D% *.exe *.dll /NFL /NDL /NJH /NJS /NP >nul
if not exist %D%\root mkdir %D%\root
for %%X in (mods python Profiles Settings) do if not exist "%D%\root\%%X" mklink /J "%D%\root\%%X" "C:\Battlefield 2\%%X" >nul
for %%H in (home-demoret home-demo86) do (
  if exist C:\rendfw\%%H rmdir /s /q C:\rendfw\%%H
  robocopy C:\rendfw\home-template C:\rendfw\%%H /E /NFL /NDL /NJH /NJS /NP >nul
)
dir /b %D%
