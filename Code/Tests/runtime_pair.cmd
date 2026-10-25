@rem SCAFFOLDING TEST - not recovered source and not part of the BF2 build.
@rem Builds and runs the focused SimpleObject/Bundle runtime-pair validation harness.
@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86
pushd "%~dp0"
cl /nologo /EHsc /O2 /std:c++14 /permissive /Gy /DDEDICATED=1 /FI"%~dp0..\BF2\Core\BuildConfig.h" runtime_pair.cpp ^
  ..\BF2\Game\Objects\IObject.cpp ^
  ..\BF2\Game\Objects\IObjectComponent.cpp ^
  ..\BF2\Game\Objects\IObjectTemplate.cpp ^
  ..\BF2\Game\Objects\ObjectTemplate.cpp ^
  ..\BF2\Game\Objects\Base\SimpleObjectTemplate.cpp ^
  ..\BF2\Game\Objects\Base\BundleTemplate.cpp ^
  ..\BF2\Game\Objects\ItemContainerTemplate.cpp ^
  ..\BF2\Game\Objects\Base\SimpleObject.cpp ^
  ..\BF2\Game\Objects\Base\Bundle.cpp ^
  ..\BF2\Game\Objects\Visual\LightSource.cpp ^
  ..\BF2\Game\Objects\Visual\LightSourceTemplate.cpp ^
  ..\BF2\Scene\Object\WorldObjTemplPortal\PortalManager.cpp ^
  ..\BF2\Scene\Object\Object.cpp ^
  ..\BF2\Scene\Object\ObjectPredicator.cpp ^
  ..\BF2\Scene\Object\ObjectUtil.cpp ^
  ..\BF2\Scene\Object\ObjectTypes\Game\IPlayerControlObject.cpp ^
  ..\BF2\Game\Utility\Vars.cpp ^
  ..\BF2\Physics\IPhysicsNode.cpp ^
  ..\BF2\Physics\IResponsePhysics.cpp ^
  ..\BF2\Physics\GeomUtil.cpp ^
  ..\BF2\Physics\PhysicsNode.cpp ^
  ..\BF2\Physics\PhysicsType.cpp ^
  ..\BF2\Physics\PhysicsUtil.cpp ^
  ..\BF2\Physics\PointPhysicsNode.cpp ^
  ..\BF2\Physics\PointResponsePhysics.cpp ^
  ..\BF2\Physics\ResponsePhysics.cpp ^
  ..\BF2\Physics\ResponsePhysicsManager.cpp ^
  ..\BF2\Physics\RotationalPointPhysicsNode.cpp ^
  ..\BF2\Physics\RotationalPointResponsePhysics.cpp ^
  ..\BF2\Physics\SoldierPhysicsNode.cpp ^
  ..\BF2\Physics\SoldierResponse.cpp ^
  ..\BF2\Physics\StaticPhysicsNode.cpp ^
  ..\BF2\Physics\StaticResponsePhysics.cpp ^
  ..\BF2\IO\Network\INetworkable.cpp ^
  ..\BF2\Math\LocalBoundingBox.cpp ^
  ..\BF2\Math\Frustum.cpp ^
  ..\BF2\Math\Mat4.cpp ^
  ..\BF2\Math\Plane3.cpp ^
  ..\BF2\Math\Quat.cpp ^
  ..\BF2\Math\Vec3.cpp ^
  ..\BF2\Core\StringUtil.cpp ^
  ..\BF2\Core\Debug.cpp ^
  ..\BF2\Core\Globals.cpp ^
  /link /OPT:REF /OUT:runtime_pair.exe > runtime_pair_build.log 2>&1
if errorlevel 1 ( type runtime_pair_build.log & popd & exit /b 1 )
"%~dp0runtime_pair.exe"
set RC=%errorlevel%
popd
exit /b %RC%
