@echo off
setlocal EnableExtensions EnableDelayedExpansion
rem [TEMP] Native x86 /Od + /O2 twin of the macOS CTest bf2_slider_node_check: the recovered SliderNode
rem and BfFixedSliderNode frame/event bodies (slider_node_behaviour.cpp). /FORCE:UNRESOLVED covers
rem the BFMeme families the fixture never calls.
call "%~dp0..\Build\vsdevcmd.cmd" x86
set "TASK_ROOT=%~dp0..\.."
if not defined BF2_BUILD_OUT set "BF2_BUILD_OUT=%~dp0..\Build\obj_slider_node_behaviour_20261112"

call :build_and_run od /Od
if errorlevel 1 exit /b 1
call :build_and_run o2 /O2
if errorlevel 1 exit /b 1
echo slider node behaviour passed at /Od and /O2
exit /b 0

:build_and_run
set "TASK_TAG=%~1"
set "TASK_OPT=%~2"
set "TASK_OUT=%BF2_BUILD_OUT%\%TASK_TAG%"
if not exist "%TASK_OUT%" mkdir "%TASK_OUT%"
set "TASK_FLAGS=/nologo /c /MD /std:c++14 /permissive /EHsc !TASK_OPT! /Z7 /Gy /DBF2_PLUGIN_ABI_NATIVE=1 /D_ITERATOR_DEBUG_LEVEL=0 /DDEDICATED=0"
set "TASK_OBJECTS="
for %%S in (Object TypeDescriptor Node NodeState System Data NamedObjectRegistry Stream SliderNode) do (
  cl !TASK_FLAGS! /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" "%TASK_ROOT%\Code\BF2\External\BFMeme\%%S.cpp" /Fo"%TASK_OUT%\%%S.obj"
  if errorlevel 1 exit /b 1
  set "TASK_OBJECTS=!TASK_OBJECTS! "%TASK_OUT%\%%S.obj""
)
cl !TASK_FLAGS! /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" "%TASK_ROOT%\Code\BF2\Menu\BfFixedSliderNode.cpp" /Fo"%TASK_OUT%\BfFixedSliderNode.obj"
if errorlevel 1 exit /b 1
cl !TASK_FLAGS! /FI"%TASK_ROOT%\Code\BF2\Core\BuildConfig.h" "%~dp0slider_node_behaviour.cpp" /Fo"%TASK_OUT%\fixture.obj"
if errorlevel 1 exit /b 1
link /nologo /FORCE:UNRESOLVED /OPT:REF /INCREMENTAL:NO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"%TASK_OUT%\slider_node_behaviour.exe" !TASK_OBJECTS! "%TASK_OUT%\BfFixedSliderNode.obj" "%TASK_OUT%\fixture.obj" >"%TASK_OUT%\link.log" 2>&1
if not exist "%TASK_OUT%\slider_node_behaviour.exe" (
  type "%TASK_OUT%\link.log"
  exit /b 1
)
"%TASK_OUT%\slider_node_behaviour.exe"
set "TASK_EXIT=!errorlevel!"
echo slider_node_behaviour.exe (%TASK_TAG%) exit !TASK_EXIT!
if not "!TASK_EXIT!"=="0" (
  echo unresolved externals of the %TASK_TAG% link:
  findstr /c:"unresolved external" "%TASK_OUT%\link.log"
  exit /b 1
)
exit /b 0
