@echo off
rem The editor's RendDX9 step (Code/Tools/BF2Editor/CMakeLists.txt, bf2_editor_renddx9): the tree's
rem renddx9.cmd native, redirected into the editor's build tree by BF2_RENDDX9_OUT / BF2_RENDDX9_BIN -
rem or, with BF2_EDITOR_KEEP_RENDDX9=1 (build_editor.ps1 -KeepRendDX9, while another agent's shared
rem renderer change is in flight), the DLL already built is kept, and the log says it may be older than
rem its sources.
rem Then the TextureAtlasBuilder.dll the rebuilt RendDX9.dll imports, as the shipped one does (RW-278):
rem the tree's textureatlasbuilder.cmd native, redirected by BF2_TEXTUREATLAS_OUT / BF2_TEXTUREATLAS_BIN.
rem CMake stages it beside BF2Editor.exe, where the loader looks for RendDX9.dll's imports first; without
rem it RendDX9.dll does not load. Kept too under BF2_EDITOR_KEEP_RENDDX9=1 once it exists.
rem Failures leave through :fail (an exit /b 1 inside a block reaches a cmd /c caller as 0); CRLF file.
rem usage: renddx9_for_editor.cmd [native|x64] - the flavour both scripts build (default native, x86).
set FLAVOUR=native
if /i "%~1"=="x64" set FLAVOUR=x64
rem The editor's flavour: the editor image define admits the editor-only regions of shared RendDX9 code.
set BF2_RENDDX9_EDITOR=1
set KEEP_RENDDX9=0
if "%BF2_EDITOR_KEEP_RENDDX9%"=="1" if exist "%BF2_RENDDX9_BIN%\RendDX9.dll" set KEEP_RENDDX9=1
if "%KEEP_RENDDX9%"=="1" echo [bf2_editor_renddx9] KEPT the RendDX9.dll already built ^(BF2_EDITOR_KEEP_RENDDX9=1^); it may be older than its sources
if "%KEEP_RENDDX9%"=="1" goto :atlas
call "%~dp0..\..\Build\renddx9.cmd" %FLAVOUR%
if errorlevel 1 goto :fail
:atlas
set KEEP_ATLAS=0
if "%BF2_EDITOR_KEEP_RENDDX9%"=="1" if exist "%BF2_TEXTUREATLAS_BIN%\TextureAtlasBuilder.dll" set KEEP_ATLAS=1
if "%KEEP_ATLAS%"=="1" echo [bf2_editor_renddx9] KEPT the TextureAtlasBuilder.dll already built
if "%KEEP_ATLAS%"=="1" exit /b 0
call "%~dp0..\..\Build\textureatlasbuilder.cmd" %FLAVOUR%
if errorlevel 1 goto :fail
exit /b 0
:fail
echo [bf2_editor_renddx9] FAILED
exit /b 1
