@rem ===========================================================================
@rem SCAFFOLDING - NOT RECOVERED SOURCE. NOT PART OF THE BF2 BUILD.
@rem
@rem Reproduces the MSVC syntax-only check of the recovered Game/Main sources.
@rem /Zs = parse and semantic-check only, no codegen and no link, so the stub
@rem headers next to this file need no definitions at all.
@rem /permissive and /std:c++14 stop a 2026 compiler rejecting legitimate
@rem early-2000s C++.
@rem /FI force-includes the stub bundle so that the recovered .cpp files, which
@rem reference dozens of engine names without including any header, can be
@rem checked WITHOUT editing anything under Code\BF2\.
@rem
@rem   usage:  syntaxcheck.cmd                 (all three files)
@rem           syntaxcheck.cmd <path\to.cpp>   (one file)
@rem ===========================================================================
@echo off
setlocal
call "%~dp0..\Build\vsdevcmd.cmd" x86

set STUBS=%~dp0
set SRC=%~dp0..\BF2\Game\Main
set INC=/FI"%STUBS%_prelude.h" /I"%STUBS%." /I"%STUBS%shadow" /I"%STUBS%shadow\Game\Main"

@rem The embedded CPython 2.3.4 headers from External/. MOVED UP HERE 2026-07-29 - it used to be set down at
@rem the PythonHost.cpp line, which was fine while only Game\Python\ needed it. BF2Engine.cpp now
@rem includes Game/Python/PythonHost.h (for the per-frame pythonHost()->update() call), and that
@rem header includes <Python.h>; its cl line runs long BEFORE the old definition point, so the
@rem variable would have expanded to nothing and the file would have failed on a missing include.
@rem Py_NO_ENABLE_SHARED must match how the library is built - see Code\Build\python\build_python.cmd.
set PYINC=/DPy_NO_ENABLE_SHARED /I"%~dp0..\BF2\External\python-embedded\py-src-2.3.4\Include" /I"%~dp0..\BF2\External\python-embedded\py-src-2.3.4\PC"

@rem Real headers can pull these third-party APIs in transitively (for example FileManager ->
@rem ZipArchive and GameServer -> PythonHost). Keep the same common include surface as build.cmd
@rem instead of trying to predict which recovered translation unit needs it directly.
set ZIPINC=/I"%~dp0..\BF2\External\zlib-1.2.3" /I"%~dp0..\BF2\External\zlib-1.2.3\contrib\minizip"
@rem /DDEDICATED=1 + /FI Core\BuildConfig.h, 2026-09-02: the image define - this gate checks the DEDICATED
@rem configuration. Client-only regions live under #if !DEDICATED and the header errors without the define.
@rem A second pass with /DDEDICATED=0 arrives with the first guarded TU.
set FLAGS=/nologo /Zs /std:c++14 /permissive /EHsc %PYINC% %ZIPINC% /DDEDICATED=1 /FI"%~dp0..\BF2\Core\BuildConfig.h"

@rem TWO GROUPS, and they must not be mixed:
@rem
@rem   GROUP A (Game\Main) - these .cpp files include no engine headers at all, so they need the
@rem   stub bundle force-included to resolve the names they reference.
@rem
@rem   GROUP B (everything below) - these .cpp files include their own REAL headers. Force-including
@rem   the stubs here causes C2011 'class type redefinition' against the real declarations, so
@rem   group B is compiled WITHOUT /FI and without the stub include path. As real headers replace
@rem   stubs, files move from group A to group B.

@rem FAILURES ARE ACCUMULATED. Each cl invocation's exit code is folded into RC, and the script
@rem exits non-zero if ANY file failed. Without this the script returned the exit code of the LAST
@rem compile only, so a failure anywhere but the end was reported as success - which it did, silently,
@rem while Console.cpp had two C2065 errors.
set RC=0

@rem NB: the accumulation is written out longhand rather than via a :subroutine. A `call :one` helper
@rem that shifted its arguments silently dropped the quoted /FI and /I flags in %INC%, which made
@rem group A fail with ~100 bogus "undeclared identifier" errors. Longhand is uglier and correct.

@rem Single-file mode. It used to ALWAYS pass %INC%, i.e. force-include the stub bundle - which is
@rem correct for group A but produces a wall of bogus C2011 'type redefinition' errors for group B
@rem files that include their own real headers. Pass a second argument "stubs" to opt in.
@rem     syntaxcheck.cmd <file>            group B (no stubs)   <- the common case now
@rem     syntaxcheck.cmd <file> stubs      group A
if not "%~1"=="" (
    if /i "%~2"=="stubs" ( cl %FLAGS% %INC% "%~1" ) else ( cl %FLAGS% "%~1" )
    rem Do not use %%errorlevel%% inside this parenthesized block: cmd expands it before cl runs.
    if errorlevel 1 exit /b 1
    exit /b 0
)

@rem == GROUP A IS NOW EMPTY ==
@rem All three Game\Main files have moved to group B. Every recovered .cpp in this tree now
@rem includes its own real headers, so nothing needs the forced stub bundle any more. _prelude.h and
@rem the shadow\ tree are kept because several shadows still redirect to real headers and because the
@rem retirement notes in them record what each stub once knew - but nothing compiles against them.

@echo == group B: real headers, no stubs ==
@rem BF2Engine.cpp MOVED FROM GROUP A TO GROUP B on 2026-07-27 - the migration this file's own
@rem comment anticipated ("As real headers replace stubs, files move from group A to group B").
@rem It now includes 17 real recovered headers instead of relying on the forced stub bundle, which
@rem is what finally let it COMPILE rather than merely parse: it is in the real build's object list
@rem for the first time. Force-including the stubs here now produces C2011 against those headers.
cl %FLAGS% "%SRC%\BF2.cpp"
if errorlevel 1 set /a RC+=1
@rem %PYINC% since 2026-07-29: BF2Engine.cpp includes PythonHost.h for mainLoop's python slice.
cl %FLAGS% %PYINC% "%SRC%\BF2Engine.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%SRC%\BF2EngineSetup.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Main\Game.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\Object.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\ObjectManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\ObjectCache.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ParticleSystemTemplates.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ParticleSystemTemplateProperties.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\ObjectUtil.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\ObjectTypes\Base\ObjectTemplateManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\File\FileManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\ObjectTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\IObjectTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\IObject.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\StringUtil.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Console\Console.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Console\ConsoleCompiler.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Console\Manager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\GameLogic.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\VoiceMessages.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\CombatArea.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\CombatAreaManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\CombatAreaConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\Heightmap.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\SoldierResponse.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\DemoRecorder\DemoRecorder.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\DemoRecorder\DemoSpectatorCamera.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Base\Camera.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\EffectBundle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\WheelEffectBundle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\WheelEffectBundleTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Spring.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ExplosionEffectFlashBang.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ExplosionEffectTearGas.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ExplosionEffectMolotovCocktail.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Input\InputManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\ServerSettings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\ServerSettingsConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\VoipServerConnection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\ReservedSlots.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\AutoRecorder.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\MapListConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Utility\XMLDocument.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Network\PacketFilter.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Network\NetworkLayer\Socket\UDPSocket.cpp"
if errorlevel 1 set /a RC+=1

@rem The networking implementation. These are the first .cpp files written from the 2026-07-27
@rem recovery: every body in them was read end to end in bf2.i64 rather than inferred.
@rem The socket layer initNetwork builds - recovered 2026-07-30.
cl %FLAGS% "%~dp0..\BF2\IO\Network\SocketManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Network\INetworkable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Network\NetworkableBase.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Network\NetworkableInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\ScoreSystem\TeamScore.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\WeaponComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\BitStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\ChecksumContext.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\IObjectComponent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Base\SimpleObjectTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Base\BundleTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Base\SimpleObject.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Base\Bundle.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\DestroyableWindow.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\DestroyableWindowsBundle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\MaskObjectTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\MaskObject.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\KitPart.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\SupplyDepot.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Triggerable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\TriggerableTarget.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\SpriteParticle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\Emitter.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\Particle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ParticleTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ModuleWorldObjTemplFx.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ModuleWorldObjTemplFxEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Fx\ModuleWorldObjTemplFxParticles.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\ObjectPriorityCalculator.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\File\BufferedStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\DynamicBundle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\AreaObject.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Obstacle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\ItemContainerTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Item.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Kit.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\OrderMarker.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Math\Mat4.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Math\LocalBoundingBox.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Math\Vec3.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Math\Plane3.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\DeviationComponents\DeviationCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\GameLogicComp\ArmorGLComp.cpp"
if errorlevel 1 set /a RC+=1

@rem header_check.cpp is SCAFFOLDING: it includes the 35 recovered headers that no translation unit
@rem reached, so they are compiled rather than merely written. Adding it immediately found two real
@rem defects (a Vec3 forward declaration conflicting with the Math/Vec3.h typedef, and
@rem SoldierTemplate.h using Vec3 without including it).
cl %FLAGS% "%~dp0header_check.cpp"
if errorlevel 1 set /a RC+=1
@rem The BF2VoipServer public headers (Phase 0 declarations). They are not reached by an engine
@rem translation unit yet, so this scaffolding TU compiles them the way header_check.cpp does.
cl %FLAGS% "%~dp0voip_header_check.cpp"
if errorlevel 1 set /a RC+=1
@rem The BF2VoipServer image's exported facade TU. Phase 0 bodies are named [FRONTIER] stops.
cl %FLAGS% "%~dp0..\BF2\VoipServer\VoipServer.cpp"
if errorlevel 1 set /a RC+=1
@rem The VOIP container library (Phase 1).
cl %FLAGS% "%~dp0..\BF2\VoipServer\VoipClients.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\VoipServer\VoipOutgoingChannels.cpp"
if errorlevel 1 set /a RC+=1
@rem The VOIP routing core (Phase 2).
cl %FLAGS% "%~dp0..\BF2\VoipServer\VoipRouter.cpp"
if errorlevel 1 set /a RC+=1
@rem The voice-server instance, its diagnostics and the Timer dependency (Phase 3).
cl %FLAGS% "%~dp0..\BF2\VoipServer\VoipDebug.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\VoipServer\VoipServerInstance.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\Timer.cpp"
if errorlevel 1 set /a RC+=1
@rem The in-process voice server + the engine wiring that drives it (Phase 7).
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\VoipServerInternal.cpp"
if errorlevel 1 set /a RC+=1
@rem The standalone shell entry point (the BF2VoipServerShell.cpp equivalent).
cl %FLAGS% "%~dp0..\BF2\VoipServer\BF2VoipServerShell.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\Skeleton.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\Ik2bSolver\AwPoint.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\Ik2bSolver\AwVector.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\Ik2bSolver\AwQuaternion.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\Ik2bSolver\Ik2bSolver.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\BundlePlayer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\CompressedAnim.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\RagDoll.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFGame.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\BspBuilder.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Math\TriangleToAABBIntersection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\CollisionManager.cpp"
cl %FLAGS% "%~dp0..\BF2\Physics\LatencyCompensation.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\PlayerControlObject.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\GenericProjectile.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\GenericFireArm.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\ZoomComponents\ZoomComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\ZoomComponents\DefaultZoomComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\FireComponents\FireComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\FireComponents\SingleFireComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\FireComponents\MultiFireComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\FireComponents\LPFireComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\AmmoComponents\AmmoComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\AmmoComponents\DefaultAmmoComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\AmmoComponents\ReplenishingAmmoComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\RecoilComponents\WeaponBasedRecoilComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\RecoilComponents\SoldierBasedRecoilComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\DeviationComponents\DeviationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\DeviationComponents\SimpleDeviationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\DeviationComponents\SoldierDeviationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\DeviationComponents\TargetDeviationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\TargetComponents\TargetComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\TargetComponents\SimpleTargetComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\TargetComponents\MultiFireArmTargetComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\TargetComponents\LPTargetComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\ZoomComponents\ToggleCameraComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\AnimationComponents\AnimationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\AnimationComponents\DefaultAnimationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\AnimationComponents\ThrownAnimationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\SoundComponents\DefaultSoundComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\CollisionComponents\DefaultCollisionComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\CollisionComponents\ReplenishCollisionComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\CollisionComponents\ResurrectCollisionComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\CollisionComponents\StickyCollisionComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\SeekComponents\FixedTargetComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\SeekComponents\SeekClosestTargetComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\SoundComponents\DefaultProjectileSoundComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Sound\BfSound.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Sound\SoundObjectTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\PenetrateComponents\DefaultPenetrateComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\DetonationComponents\DefaultDetonationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\DetonationComponents\ReplenishDetonationComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\FollowComponents\PreDestinedFollowComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\FollowComponents\DefaultFollowComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\RicochetComponents\DefaultRicochetComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\ModuleProjectileComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\FireComponents\SpawnObjectFireComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\FireComponents\ThrownFireComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\FireComponents\ExplosivesFireComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\Random.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Soldier.cpp"
if errorlevel 1 set /a RC+=1
@rem ADDED 2026-09-22 with Rotor::handleVisualUpdate and AntennaObject::handleVisualUpdate. The
@rem recursive build discovers both TUs but the gate never listed either, so nothing here would
@rem have caught a break in them until a full build ran.
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Rotor.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\AntennaObject.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\SoldierTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\SoldierAnimation.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\PlayerStats.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\SpawnSystem\SpawnManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Common\ServerConnection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Common\IClientConnection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Common\GhostManager.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\KitNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\DestroyableWindowsBundleNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\DestroyableObjectNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\TargetObjectNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\SpringNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\OverheadCameraNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\FreeCameraNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\ItemNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\PlayerControlObjectNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\RotationalBundleNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\SimpleObjectNetworkable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\TriggerableTargetNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\GenericProjectileNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\EngineNetworkable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\SoldierNetworkable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\KitTemplate.cpp"
if errorlevel 1 set /a RC+=1

@rem Content-specific traversal/gadget tier: each TU has its own error check so a later successful
@rem compile cannot mask an earlier failure.
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Zipline.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\ZiplineRope.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\ZiplineContainer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Ladder.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\LadderContainer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\GrapplingHookRope.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\GrapplingHookRopeContainer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\RopeLink.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameEvents\GrapplingHookContainerCreateEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameEvents\GrapplingHookContainerDetachEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameEvents\GrapplingHookContainerUpdateEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameEvents\GrapplingHookCreateEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameEvents\GrapplingHookUpdateEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameEvents\TargetDirectionEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\TemplateLeaves.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\AmbientEffectArea.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameEvents\AmbientEffectAreaEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\LandingGearTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\LandingGear.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\WindAffector.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\IkTemplateComponent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\StrategicObjectComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\StrategicObjectCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\PostProcessCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\VehicleHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\WeaponHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\AbilityHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\WarningHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\HelpHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\HudComp\ModuleHudComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\PlayerControlObjectTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\PhysicsUtil.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\PhysicsType.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\GenericFireArmTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Networkables\GenericProjectileNetworkable.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Projectiles\GenericProjectileTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Weapons\ModuleWeapon.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Parachute.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Input\InputMapStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Vehicle\ModuleWorldObjTemplVehicle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Common\GameEventManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Common\VoteEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Common\ClientVoteManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\Vote.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\ServerVoteManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\BanList.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\BanListConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Main\OpenBF2Console.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Common\DataBlockManager.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\Common\StringManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\Player.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\ServerGameLogic.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\ClientGameLogic.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\GameLogicCollision.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\GameLogicDamage.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\SquadManager\SquadManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\SquadManager\ClientSquadManager.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\SquadManager\CommanderManager.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\SquadManager\Squad.cpp"
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\SquadManager\Team.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\ScoreSystem\ScoreManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\EndOfRoundInfo.cpp"
if errorlevel 1 set /a RC+=1
@rem PythonHost.cpp needs the CPython 2.3.4 headers (%PYINC%, now set at the top of this file).
cl %FLAGS% %PYINC% "%~dp0..\BF2\Game\Python\PythonHost.cpp"
if errorlevel 1 set /a RC+=1
@rem NetClient.cpp, GameClient.cpp (2026-09-14) and CdKeyStore.cpp (the CD-key store handleChallenge
@rem reads, 2026-09-14) are CLIENT-ONLY bodies, whole-file #if !DEDICATED: under /DDEDICATED=1 each
@rem compiles to an empty translation unit, and that is what these rows prove - the guard holds and
@rem none of them leaks a symbol into the dedicated image.
cl %FLAGS% "%~dp0..\BF2\IO\Network\ProtocolLayer\NetClient.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameClient\GameClient.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\MatchMaking\Client\CdKeyStore.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Network\ProtocolLayer\NetServer.cpp"
cl %FLAGS% "%~dp0..\BF2\IO\Network\ProtocolLayer\PacketQueue.cpp"
cl %FLAGS% "%~dp0..\BF2\IO\Network\ProtocolLayer\PacketBuffer.cpp"
cl %FLAGS% "%~dp0..\BF2\IO\Network\ProtocolLayer\SWConnection.cpp"
cl %FLAGS% "%~dp0..\BF2\IO\Network\ProtocolLayer\NetServerThread.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\ConsoleObjectBase.cpp"
if errorlevel 1 set /a RC+=1
@rem The two singleton classes BF2EngineSetup::initModules creates AFTER the 26-module init loop:
@rem setSpawnManager then world::setMaterialManager. Both were BF2_FACTORY shims resolving to RVA 0.
@rem SpawnManager.cpp already had a cl line (it was surface-only until 2026-07-30); this is the new TU.
cl %FLAGS% "%~dp0..\BF2\Physics\MaterialManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\MMCell.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\MaterialSystem\ModuleMaterialSystem.cpp"
if errorlevel 1 set /a RC+=1
@rem The ScoreManager console table - the 22 bindings Settings\ScoreManagerSetup.con writes.
cl %FLAGS% "%~dp0..\BF2\Game\GameLogic\ScoreSystem\ModuleScoreSystem.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\GameLogicComp\ModuleGameLogicComp.cpp"
if errorlevel 1 set /a RC+=1
if errorlevel 1 set /a RC+=1

@rem The Windows half of the filesystem pair - the FIRST class recovered from the PE rather than the
@rem ELF (bf2_w32ded.exe.i64). UnixFileSystem.cpp is POSIX-guarded and compiles to nothing here, so
@rem without this the engine cannot obtain a filesystem at all.
cl %FLAGS% "%~dp0..\BF2\IO\File\Win32FileSystem.cpp"
if errorlevel 1 set /a RC+=1
@rem initFileSystem's SECOND create. Platform-neutral, so recovered from the ELF, not the PE.
@rem The zip TUs need the vendored zlib/minizip headers (External\zlib-1.2.3) on the include path.
set ZIPINC=/I"%~dp0..\BF2\External\zlib-1.2.3" /I"%~dp0..\BF2\External\zlib-1.2.3\contrib\minizip"
cl %FLAGS% %ZIPINC% "%~dp0..\BF2\IO\File\ZipArchiveHandler.cpp"
if errorlevel 1 set /a RC+=1
@rem The concrete zip layer, recovered 2026-08-14: ZipArchive + its FileHandle pool + ZipReadStream
@rem over stock minizip 1.01e. The whole TU is Debug-site attested as IO/File/ZipArchive.cpp.
cl %FLAGS% %ZIPINC% "%~dp0..\BF2\IO\File\ZipArchive.cpp"
if errorlevel 1 set /a RC+=1
@rem The zlib stream pair (2026-09-08): ZUtil.cpp is Debug-site attested; ZStream is its ELF-named
@rem IStream wrapper; DemoHeader is the demo file's header both images serialize.
cl %FLAGS% %ZIPINC% "%~dp0..\BF2\IO\ZUtil.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Stream\ZStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\DemoRecorder\DemoHeader.cpp"
if errorlevel 1 set /a RC+=1
@rem The growable memory stream ZipArchive::open inflates small entries into (CID_MemStream).
cl %FLAGS% "%~dp0..\BF2\IO\Stream\MemStream.cpp"
if errorlevel 1 set /a RC+=1
@rem The RAII file handle OldConsole::run drives. Recovering it also promoted io::IFile from a
@rem forward declaration to a real polymorphic interface - see IFileSystem.h.
cl %FLAGS% "%~dp0..\BF2\IO\File\File.cpp"
if errorlevel 1 set /a RC+=1

@rem The console registry itself, recovered 2026-07-30 - the boot blocker at Module::init+0x4D. It
@rem replaced the three local ConsoleObjects shims in Core\Module.cpp, Core\ModuleRegistry.cpp and
@rem Game\Main\BF2Console.cpp, all three of which are re-checked above.
cl %FLAGS% "%~dp0..\BF2\Core\ConsoleObjects.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\ClientInfo.cpp"
if errorlevel 1 set /a RC+=1
@rem The four data-block receivers (2026-09-14): shared TUs whose client halves were written that day
@rem (ServerInfo::updateFromDataBlock, MapInfo::setFromDataBlock, MapList::updateFromDataBlock, and the
@rem ContentCheckEvent ctor that now reads io::ChecksumContext directly). They had never been in this gate.
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\ServerInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\MapInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\MapList.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameEvents\ContentCheckEvent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\AntiHacks.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\KeyParser.cpp"
if errorlevel 1 set RC=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\LeaderBoard.cpp"
if errorlevel 1 set RC=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\SearchForPlayer.cpp"
if errorlevel 1 set RC=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\PlayerRankInfo.cpp"
if errorlevel 1 set RC=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\PlayerUnlockInfo.cpp"
if errorlevel 1 set RC=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\StatsManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\PlayerStatsManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\PlayerUnlocks.cpp"
if errorlevel 1 set RC=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\PlayerAwards.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\HTTPRequest.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Persistence\GSServerSideNameValidation.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\GameServer\GameServer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\StickyProjectileManager.cpp"
if errorlevel 1 set /a RC+=1
@rem Commander strategic-tool ownership, artillery and the managed UAV drone, recovered 2026-09-03.
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\CommanderToolsManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\ArtilleryHandler.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\UAVVehicleManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\DropVehicle.cpp"
if errorlevel 1 set /a RC+=1

@rem The dedicated-server status screen. StatusMonitorImpl\posix.cpp is ncurses and compiles to an
@rem empty translation unit here - it is listed anyway so that a broken platform guard is caught
@rem rather than silently skipped.
cl %FLAGS% "%~dp0..\BF2\Game\Main\StatusMonitor.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Main\StatusMonitorImpl_win32.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Main\StatusMonitorImpl\posix.cpp"
if errorlevel 1 set /a RC+=1
@rem initMath/closeMath - MathModule::init's callees, recovered 2026-07-30 by walking the boot
@rem harness forward from ModuleGameLogic::init.
cl %FLAGS% "%~dp0..\BF2\Math\Math.cpp"
if errorlevel 1 set /a RC+=1

@rem Both of these were excluded for a while pending Core\Ids.h, Core\Profiler.h and the four
@rem singleton globals. All now exist, and adding them back surfaced six further real defects -
@rem see the file headers for each. Everything recovered is now checked.
cl %FLAGS% "%~dp0..\BF2\Core\ClassManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\Module.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\ModuleRegistry.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\Registry.cpp"
if errorlevel 1 set /a RC+=1

@rem The 2026-07-28 recovery batch. Found MISSING from this gate by the code-vs-IDB audit of
@rem 2026-07-28 - the driver had not been updated for the newest files, so "Everything recovered
@rem is now checked" (above) had silently stopped being true. Rule restated: when a recovered .cpp
@rem lands, its cl line lands in the same change.
cl %FLAGS% "%~dp0..\BF2\Core\Globals.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\LoadStat.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\Mutex.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\Thread.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Main\BF2Console.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Main\BF2Log.cpp"
if errorlevel 1 set /a RC+=1
@rem Same CPython include requirement as PythonHost.cpp above.
cl %FLAGS% %PYINC% "%~dp0..\BF2\Game\Python\py\Objects.cpp"
if errorlevel 1 set /a RC+=1
@rem POSIX-guarded like StatusMonitorImpl\posix.cpp - listed so a broken guard is caught.
cl %FLAGS% "%~dp0..\BF2\IO\File\UnixFileSystem.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\SettingsManager.cpp"
if errorlevel 1 set /a RC+=1

@rem The 2026-07-29 mainLoop-dependency batch: the four subsystems BF2Engine::mainLoop dispatches
@rem to every frame. Added in the same change as the code, per this file's own standing rule.
cl %FLAGS% "%~dp0..\BF2\Core\EventManager.cpp"
if errorlevel 1 set /a RC+=1
@rem The five middleware licence notices - global-scope, called from BF2Engine's ctor. Found by the
@rem boot harness (Code\Build\bootrun.cmd), which faulted inside that ctor on 2026-07-29.
cl %FLAGS% "%~dp0..\BF2\Core\CreditNotices.cpp"
if errorlevel 1 set /a RC+=1

@rem The BF2Engine::init frontier, recovered 2026-07-29 by walking the boot harness forward. Each of
@rem these was an unwritten function that init calls; the harness named them in execution order.
cl %FLAGS% "%~dp0..\BF2\Core\SettingsRepostitory.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\SystemInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\File\FileChangeManager.cpp"
if errorlevel 1 set /a RC+=1

@rem The three Core-module singletons whose create() the boot actually invokes through
@rem ClassManager::createSingleton. Found by walking the boot harness forward, 2026-07-29 - NOT by
@rem reading the link report, which is dominated by the 124 ClassFactory<T> rows that registration
@rem only ever STORES and never calls.
cl %FLAGS% "%~dp0..\BF2\Core\MemoryPool.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\Malloc.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\Profiler.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\DebugCounterManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\PluginDll.cpp"
if errorlevel 1 set /a RC+=1

@rem The singleton FACTORIES, 2026-07-29. These are the create() functions ClassManager actually
@rem invokes as each module registers - found by walking the boot harness, in registration order.
@rem The last call in BF2Engine::initEngine - ATTESTED (anon-ns) path, recovered 2026-07-30.
cl %FLAGS% "%~dp0..\BF2\Game\SingletonServer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\ModInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Input\ControlMap.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Input\InputEnums.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Input\ControlMapManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Input\ControlMapConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Input\NullInputDeviceManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Resource\NullResourceManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Resource\NullCacheManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Stream\AsyncMultiStreamer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\Localization\ExcelLexicon.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\PlayerManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\PlayerScoreData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\WindManager.cpp"
if errorlevel 1 set /a RC+=1
@rem The last three singletons ClassManager::initSingletons constructs (2026-07-29). CollisionManager
@rem and Heightmap already had .cpp entries above; only HeightmapCluster is a new translation unit.
cl %FLAGS% "%~dp0..\BF2\Physics\HeightmapCluster.cpp"
if errorlevel 1 set /a RC+=1
@rem The editable heightmap pair and their event node (2026-09-30, the editor's WP8).
cl %FLAGS% "%~dp0..\BF2\Physics\HeightmapEditable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\HeightmapClusterEditable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Geom\HeightmapEventNode.cpp"
if errorlevel 1 set /a RC+=1

@rem The five boot-path ClassFactory<T> classes + AIConnection, made INSTANTIABLE 2026-07-30, and the
@rem one file that emits their template instantiations. See Core/ClassFactory.h for why the
@rem instantiation is opt-in per class rather than a header include in ModuleRegistry.cpp.
cl %FLAGS% "%~dp0..\BF2\Physics\PhysicsNodeManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\ResponsePhysicsManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Physics\BasicPhysicsSystem.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\WorldObjTemplPortal\PortalManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\Main\AIConnection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\Module\BFConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\IAIEventCourier.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\SingletonServer.cpp"
if errorlevel 1 set /a RC+=1

@rem Chunk 01's sixteen engine-side AI adapters plus the event listener (2026-09-08).
@rem ENGINE members - unlike the AIDLL block below, these DO enter bf2_w32ded.exe.
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\Main\AIEventListener.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIPlayerFactory.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIPlayer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplateManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplatePlugInManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplatePlugIn.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplatePhysical.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\InformationType.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplateArmament.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplateControlInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplateCover.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplateMobile.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AITemplateUnit.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\SoldierPose.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\WeaponTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFCollisionHandler.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFCommander.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFGame.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFGameEffector.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFGraphCreator.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFObjectEnvironment.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFPhysics.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFRadioComm.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFSquad.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFTerrain.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\BFTimer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\DebugRenderer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\KitTemplateManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\KitTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\WeaponTemplateManager.cpp"
if errorlevel 1 set /a RC+=1
@rem AI chunk 07 (2026-09-29): the engine's AIObject family, its six plug-ins, the footprint polygon
@rem and the AI weapon component (WeaponObject).
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObject.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectReal.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectShared.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectPrimaryReal.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectPrimaryShared.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectSecondaryReal.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectSecondaryShared.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectArmament.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectControlInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectCover.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectMobile.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectPhysical.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\AIObjectUnit.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\PlanePolygon.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIInterface\BFKnowledge\WeaponObject.cpp"
if errorlevel 1 set /a RC+=1

@rem Side-image coverage. These TUs are AIDLL-only and are deliberately absent from the
@rem bf2_w32ded engine-object manifest; CommonSingletonServer is the genuine shared member (its
@rem class is in BOTH shipped images, and its AIDLL-only half was split out on 2026-09-08 into
@rem AISingletonServer.cpp, which is listed below). Syntax-checking them here does NOT make them
@rem engine members - Code\Build\aidll.cmd is the real image, and this pass carries none of that
@rem build's defines, so a clean exit here says nothing about the DLL link.
cl %FLAGS% "%~dp0..\BF2\Core\DllModule.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\DllGlobals.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AISingletonServer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\BotMain.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\BotNavigation.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\BotSquad.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\SensingAgent.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\CommunicationAgent.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\BotSenses.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPlanInterpreter\BAPContext.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\InformationManager.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\Information.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\WeaponInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\IPIArmament.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\IPIArmamentReal.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\IPICover.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\IPIMobile.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\IPIMobileReal.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\IPIMobileKnown.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\IPIUnit.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\InformationReal.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\WeaponInfoReal.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\IPIPhysicalReal.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIUtils\VehicleOccupancy.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\BFInformationEnvironment.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\AIInformationGrid.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIUtils\DebugRendering\AIRenderUtil.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\WeaponInfoManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\BFInformationCourier.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIKnowledge\InformationFactory.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicMap\BFStrategicEnvironment.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicMap\BotSpawner.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\DrawPrimitives\DPManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AICollisionHandler.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AIEventCourier.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AIMineManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AIPlayerCourier.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\BFPathfindingCourier.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\Material\Material.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\Material\MaterialManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\NavMesh\MapProperties.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\NavMesh\QuadTreeManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\Cluster\ClusterNodeManager.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\NavMesh\QTSearchNodeManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\NavMesh\QuadTree.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\NavMesh\QuadTreeNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPathFinding\Cluster\ClusterGraph.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicMap\AerialHeightMap.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIPlanInterpreter\BAPEntries.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\SensingAgentPatterns.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\SquadManager.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\SquadCoordinator.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\SquadFormation.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\SquadMember.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBotBehaviour\CustomCurve.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\BotMemory.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBot\BotSubObjects.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBotBehaviour\UrgencyMerger.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AIConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIBotBehaviour\UrgeCurve.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicEntity\CoverPosition\CPNodeManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicEntity\CoverPosition\CoverPositionManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicEntity\AIStrategicObjectManager.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicEntity\AILandingZoneManager.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicEntity\AIStrategicObject.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicEntity\AIStrategicArea.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategicEntity\SAWayPoint.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\AIStrategic\SAI.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\BotBehaviourManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\CommunicationHub.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\StrategyManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AIPathfinding.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\IAIPathfinding.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\UserOrderManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AIMain.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AIModule.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AIRandom.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AILODManager.cpp"
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\IBotManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\AISettings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\BF2AI.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\BotManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\AI\AIDLL\AI\Main\CommonSingletonServer.cpp"
if errorlevel 1 set /a RC+=1

cl %FLAGS% "%~dp0..\BF2\Core\ClassFactoryInstances.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\ClassFactoryInstantiations.cpp"
if errorlevel 1 set /a RC+=1

@rem The cullers ObjectManager's constructor builds - five of them, two BinTree and three QuadTree.
@rem Both have their OWN static create(), so they are NOT ClassFactoryInstances.cpp entries.
cl %FLAGS% "%~dp0..\BF2\Scene\Object\BinTreeCuller.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Scene\Object\QuadTreeCuller.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Animation\BoneAnimation\RagDollTemplate.cpp"
if errorlevel 1 set /a RC+=1

@rem The 2026-07-28 Milestone-1 recovery: the mainLoop dispatch targets. SimpleParser and
@rem TickCalculator are complete classes; System.cpp is the attested (anon-ns) engine clock;
@rem demo.cpp is the attested (anon-ns, LOWERCASE) demo coordinator.
@rem The build/protocol version pair - ATTESTED (anon-ns) path, recovered 2026-07-30.
cl %FLAGS% "%~dp0..\BF2\Game\Utility\BuildNrUtil.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Utility\SimpleParser.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\IO\TickCalculator.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Core\System.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Main\demo.cpp"
if errorlevel 1 set /a RC+=1

@rem Core/Debug.cpp - the diagnostic behind all 593 __FILE__ sites, recovered 2026-07-28.
cl %FLAGS% "%~dp0..\BF2\Core\Debug.cpp"
if errorlevel 1 set /a RC+=1

@rem The 2026-07-28 batch-2 recovery: the console-variable store and the demo config record.
cl %FLAGS% "%~dp0..\BF2\Game\Utility\Vars.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\DemoRecorder\DemoConfig.cpp"
if errorlevel 1 set /a RC+=1

@rem == THE CLIENT CONFIGURATION, 2026-09-02. The translation units that carry #if !DEDICATED regions
@rem    are parsed a second time with /DDEDICATED=0, so a client-only region can never rot unseen the way
@rem    the BF2_IMAGE_RENDDX9 units once did (RW-50). Add a file here when it gains such a region.
@rem
set FLAGS0=/nologo /Zs /std:c++14 /permissive /EHsc /MD /D_ITERATOR_DEBUG_LEVEL=0 /DBF2_PLUGIN_ABI_NATIVE=1 %PYINC% %ZIPINC% /DDEDICATED=0 /FI"%~dp0..\BF2\Core\BuildConfig.h"
@echo == client configuration: /DDEDICATED=0 ==
@rem Bundled meshes and their reflection dependency are renderer-owned client sources.
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\BundledMesh.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\BundledMeshDraw.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\BundledMeshHeight.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\BundledMeshTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\BundledMeshMaterial.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\BundledMeshStateManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\RendMisc\EnvMapSystem\EnvMapManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\RendMisc\EnvMapSystem\EnvMapObject.cpp"
if errorlevel 1 set /a RC+=1
@rem Image-owned OverGrowth layout/virtuals, also present in renddx9.cmd's two image manifests.
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\Overgrowth\OverGrowth.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\Overgrowth\OverGrowthLoad.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\Overgrowth\OverGrowthCells.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\Overgrowth\OverGrowthCellRender.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\Overgrowth\OverGrowthRender.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\Overgrowth\OverGrowthConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\UndergrowthSystem.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\UndergrowthSystemLoad.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\UndergrowthSystemRender.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\UndergrowthSystemConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\DecalSystem\DecalManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\DecalSystem\DecalObject.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\DecalSystem\DecalTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\DecalSystem\DecalRenderer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\DecalSystem\DecalManagerConsole.cpp"
if errorlevel 1 set /a RC+=1
@rem RD1 (2026-09-30): compiled roads - GeometryTemplate.RoadCompiled, its instance, the renderer and the
@rem native-only Object.geometry.loadMesh row (renddx9.cmd builds all four).
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\RoadCompiledTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\RoadCompiled.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\RoadCompiledRenderer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\ObjectGeometryConsole.cpp"
if errorlevel 1 set /a RC+=1
@rem Roads step 2 (2026-10-01): the editable road objects - RoadTemplate, RoadEditableRenderer,
@rem SplineControlPoint, EditorSpline and PerlinNoise (renddx9.cmd builds all five).
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\RoadTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\RoadEditableRenderer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\GeomMisc\PerlinNoise.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\GeomMisc\SplineControlPoint.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\GeomMisc\EditorSpline.cpp"
if errorlevel 1 set /a RC+=1
@rem Roads step 3 (2026-10-01): RoadEditable and its tessellator (RoadEditableTessellate.inl is
@rem part of the same translation unit; renddx9.cmd builds it).
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\RoadEditable.cpp"
if errorlevel 1 set /a RC+=1
@rem TerrainEditable R1 (2026-10-01): the editor's terrain, its patch and ring patch, and the
@rem GetSets half (renddx9.cmd builds all four).
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\TerrainEditable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\TerrainEditableGetSets.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\QuadPatchEditable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\SurroundingPatchEditable.cpp"
if errorlevel 1 set /a RC+=1
@rem Terrain and object lightmaps (2026-10-05): the terrain light sampler, the generator, the mesh
@rem light sampler, the editor flavour's `lightmapGeneration` rows (empty without BF2_IMAGE_EDITOR, so
@rem it is checked with it too) and the native-only StaticMeshRenderer atlas rows (renddx9.cmd builds
@rem all five).
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\TerrainLightSampling.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\LightmapGeneration\LightmapGeneration.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\LightmapGeneration\MeshLightSampling.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 /DBF2_IMAGE_EDITOR=1 "%~dp0..\BF2\Geom\LightmapGeneration\LightmapGenerationConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\Geom\StaticMeshRendererConsole.cpp"
if errorlevel 1 set /a RC+=1
@rem IO/Tokenizer.cpp (RW-240): dice::hfe::Tokenizer, whose bodies compile only under BF2_IMAGE_RENDDX9 (renddx9.cmd builds it).
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\IO\Tokenizer.cpp"
if errorlevel 1 set /a RC+=1
@rem TextureAtlasBuilder.dll (2026-10-01): its six TUs compile only under BF2_IMAGE_TEXTUREATLASBUILDER
@rem (Code\Build\textureatlasbuilder.cmd builds them); RendDX9's TextureAtlas.cpp drives the DLL.
cl %FLAGS0% /DBF2_IMAGE_TEXTUREATLASBUILDER "%~dp0..\BF2\RendDX9\TextureAtlas\TextureAtlasBuilder\TextureFormat.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_TEXTUREATLASBUILDER "%~dp0..\BF2\RendDX9\TextureAtlas\TextureAtlasBuilder\Packer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_TEXTUREATLASBUILDER "%~dp0..\BF2\RendDX9\TextureAtlas\TextureAtlasBuilder\TextureAtlas.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_TEXTUREATLASBUILDER "%~dp0..\BF2\RendDX9\TextureAtlas\TextureAtlasBuilder\DllMain.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_TEXTUREATLASBUILDER "%~dp0..\BF2\RendDX9\TextureAtlas\TextureAtlasBuilder\TextureAtlasConfiguration.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_TEXTUREATLASBUILDER "%~dp0..\BF2\RendDX9\TextureAtlas\TextureAtlasBuilder\TextureAtlasD3D9.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% /DBF2_IMAGE_RENDDX9 "%~dp0..\BF2\RendDX9\TextureAtlas\TextureAtlas.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\BF2.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\BF2Engine.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\Game.cpp"
if errorlevel 1 set /a RC+=1
@rem VoiceMessages.cpp is a SHARED TU compiled into the client image too; it was only in the
@rem DEDICATED=1 pass, so its client build went unchecked. Added 2026-09-26 (playSoldierVO).
cl %FLAGS0% "%~dp0..\BF2\Game\GameLogic\VoiceMessages.cpp"
if errorlevel 1 set /a RC+=1
@rem Player.cpp likewise (shared, client-compiled, DEDICATED=1-only here until 2026-09-26).
cl %FLAGS0% "%~dp0..\BF2\Game\GameLogic\Player.cpp"
if errorlevel 1 set /a RC+=1
@rem The projectile pair likewise: GenericProjectileNetworkable::predict and its interpolation
@rem proxy are CLIENT-only bodies (RW-224), yet both TUs were DEDICATED=1-only here.
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Networkables\GenericProjectileNetworkable.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Projectiles\GenericProjectile.cpp"
if errorlevel 1 set /a RC+=1
@rem Console.cpp likewise: BF2.exe carries OldConsole::executeLine (0x6A0D50), whose "rcon " branch
@rem posts the event GameClient's ECNetwork 18 arm consumes (RW-226).
cl %FLAGS0% "%~dp0..\BF2\IO\Console\Console.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\BF2Window.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\BF2FrameInterpolator.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\BF2Render.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\BF2RenderConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerConfiguration.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerState.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerChatInput.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerCamera.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerOrders.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerRadio.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerAbilities.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerLifecycle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerUpdate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerPointer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerPlayerFilter.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerGameFilter.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerInit.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerEvents.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerHudEvents.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerFrontiers.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\ClientConsoleModules.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudConsoleBindings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\ProfileConsoleBindings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\ClientUiFrontiers.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\BF2ClientFrontiers.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\LocalProfileManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\ProfileManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\LocalProfileManagerInit.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\LocalProfileManagerOperations.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\LocalProfileManagerSelection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\LocalProfileManagerDiscovery.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\LocalProfileManagerFields.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\LocalProfileConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\SettingsConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\LocalProfileManagerSuffix.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\PasswordCodec.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\AudioSettings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\GeneralSettings.cpp"
if errorlevel 1 set /a RC+=1
@rem The six TUs GameClient::handleDataBlock reaches (2026-09-14), parsed as the CLIENT builds them. None
@rem has a #if !DEDICATED region, but ClientInfo's send half and ChecksumContext's archive half exist FOR
@rem the client, and the =1 rows alone never see them with the client's /MD and ABI defines.
cl %FLAGS0% "%~dp0..\BF2\Game\GameServer\ServerInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\GameServer\ClientInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\MapInfo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\MapList.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\IO\ChecksumContext.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\GameEvents\ContentCheckEvent.cpp"
if errorlevel 1 set /a RC+=1
@rem ClientVoteManager.cpp, 2026-09-14. NOT a client-only TU - it is shared, and the DEDICATED=1 row
@rem up at line ~458 stays. But its update / resetActiveVote / setVotingParticipationStatus bodies
@rem (client PE 599290 / 597CF0 / 5977D0) sit behind #if !DEDICATED and reach GeneralSettings,
@rem BF2HudManager and HudQuestionManager, so the =1 row alone never compiles a line of them.
cl %FLAGS0% "%~dp0..\BF2\Game\Common\ClientVoteManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\GameServerSettings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\VideoSettings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\ControlSettings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\HapticSettings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\GlobalSettings.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\DemoRecorder\DemoLibrary.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\SwiffHost.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\SwiffHostEndOfRound.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BetweenRoundsLogic.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\MainMenuLogic.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\ServerBrowser.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\ServerBrowserManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Core\ClassFactoryInstantiations.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\HudComp\ModuleHudComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\IkTemplateComponent.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\StrategicObjectComp.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\StrategicObjectCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\PostProcessCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\VehicleHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\WeaponHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\AbilityHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\WarningHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\HelpHudCompTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerGameLogicEvents.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudAwardCatalogue.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BfOccupiedVehicleData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerSpotted.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerSpottedTarget.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerCommander.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerLists.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerProjection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BF2HudManagerInput.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Math\Mat4Inverse.cpp"
if errorlevel 1 set /a RC+=1
@rem AIDLL ships the same PE fullInvert body as the client, under its dedicated image tag.
cl %FLAGS% /DBF2_IMAGE_AIDLL=1 "%~dp0..\BF2\Math\Mat4Inverse.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudScoreboard.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2MenuMapNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudBlinker.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2HudBuilder.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2SplitNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2TransformNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2CullNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2PictureNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2BarNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2CompassNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2EffectNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2BlendFuncEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2ObjectMarkerNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BfOccupiedVehicleNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2ObjectSelectionNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2HoverNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2ButtonNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2NewListBoxNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2EditNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2MiniMenuMapNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2TransformListNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\SliderNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\BfFixedSliderNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2FixedSliderNode.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2CallFunctionAction.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2ActionListAction.cpp"
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\CallFunctionAction.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2MoveEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2ColorEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2TextNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Bf2Style.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BfTransformNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudLoadingScreen.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudRadio.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudChat.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudColors.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudKitSelection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudQuestionManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudVoteMap.cpp"
cl %FLAGS0% "%~dp0..\BF2\Menu\HudVoip.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudMessages.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudCommander.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudSquad.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudAmmo.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudVehicleInformation.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudVehicleFrontiers.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BfEditData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BfEditNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BfNewListBoxNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\BfListBoxData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Ref2System.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\Font.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\DifFontMetadata.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\FontLoader.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\DifFontMetadataFile.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\System.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Object.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ObjectCollection.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Event.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Action.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Function.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\TypeDescriptor.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\NamedObjectRegistry.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Data.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Style.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Effect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\BlendFuncEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\MoveEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ColorEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\AlphaFadeEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\VariableColorEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\EffectNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ShowEffectNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Quad.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Stream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\OStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\IStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\FileIStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\FileOStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ClassOStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ClassIStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\NamedInputStream.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Node.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\NameNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\NodeState.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\PictureNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\TransformNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\CullNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ActionNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\CullVariableActionNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\BoolData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\LogicalData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\NotData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\AndData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\EqualData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\OrData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\FloatData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\IntData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\TextNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ToggleData.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\SetVariableAction.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\SetVariableSoftAction.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\SetVariableSineAction.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ObjectList.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\ActionListAction.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\SplitNode.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\Pointer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\MovePointer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\DynamicIndexPointer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\EventQueue.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\External\BFMeme\InputNodeVisitors.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Menu\HudInformationAdapter.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\VehicleHudIcons.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\HudInformationLayer.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\IO\Input\KeyboardDevice.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\IO\Input\MouseDevice.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\IO\Input\ControlMapConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\IO\Input\GameControllerDevice.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\IO\Input\FalconDevice.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\IO\Input\DX8InputDeviceManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Core\StringUtil.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Main\BF2EngineSetup.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Sound\BfSound\MusicPlayer.cpp"
if errorlevel 1 set RC=1
cl %FLAGS0% "%~dp0..\BF2\Sound\BfSound.cpp"
if errorlevel 1 set RC=1
cl %FLAGS0% "%~dp0..\BF2\Menu\MenuTeamManager.cpp"
if errorlevel 1 set RC=1
cl %FLAGS0% "%~dp0..\BF2\Menu\MenuTeamManagerData.cpp"
if errorlevel 1 set RC=1
cl %FLAGS0% "%~dp0..\BF2\Menu\MenuTeamManagerConsole.cpp"
if errorlevel 1 set RC=1
cl %FLAGS0% "%~dp0..\BF2\Menu\SwiffHostConsole.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Core\ModuleRegistry.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\Trigger.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\TriggerManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS% "%~dp0..\BF2\Game\Objects\Game\TriggerableManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Game\Trigger.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Game\TriggerManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Game\TriggerableManager.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Game\AmbientEffectArea.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\GameEvents\AmbientEffectAreaEvent.cpp"
if errorlevel 1 set /a RC+=1
@rem Work order C (2026-09-26): shared world TUs whose CLIENT compile carries code the dedicated pass
@rem never sees - the wheel-effect bundle and its template (only the client builds one), the Spring
@rem that drives them, PhysicsUtil's wheel-material read and the networkable's parachute apply.
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Fx\WheelEffectBundle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Fx\WheelEffectBundleTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Game\Spring.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Physics\PhysicsUtil.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Networkables\SimpleObjectNetworkable.cpp"
if errorlevel 1 set /a RC+=1
@rem The client's Game object, its UDP client (2026-09-14) and the CD-key store handleChallenge reads
@rem (2026-09-14): whole-body #if !DEDICATED, so this is the only configuration in which any of them
@rem compiles to anything. Game\GameClient\ has no other TU.
cl %FLAGS0% "%~dp0..\BF2\Game\GameClient\GameClient.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\IO\Network\ProtocolLayer\NetClient.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\MatchMaking\Client\CdKeyStore.cpp"
if errorlevel 1 set /a RC+=1

@rem RW-261 (2026-09-29): explosion debris and the light flash. The client compile carries what the
@rem dedicated pass never sees - Emitter::handleUpdate and the Particle init geometry arm (both
@rem #if !DEDICATED), the live createObject factories, and the new Particle / LightSource rows.
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Fx\Emitter.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Fx\Particle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Fx\ParticleTemplate.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Fx\ModuleWorldObjTemplFx.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Fx\ModuleWorldObjTemplFxEffect.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Fx\EffectBundle.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\TemplateLeaves.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Base\ModuleWorldObjTemplBase.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Visual\LightSource.cpp"
if errorlevel 1 set /a RC+=1
cl %FLAGS0% "%~dp0..\BF2\Game\Objects\Visual\LightSourceTemplate.cpp"
if errorlevel 1 set /a RC+=1

if %RC%==0 (echo ALL OK) else (echo *** %RC% FILE^(S^) FAILED ***)
exit /b %RC%
