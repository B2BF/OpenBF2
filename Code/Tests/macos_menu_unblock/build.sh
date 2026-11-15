#!/bin/sh
# [TEMP] [CHOSEN, not recovered] Offline production-boundary fixture; no client/server run.
set -eu
ROOT=$(cd "$(dirname "$0")/../../.." && pwd)
OUT="$ROOT/out/build/g4c-menu-unblock/focused"
mkdir -p "$OUT"
c++ -arch x86_64 -std=gnu++14 -fms-extensions \
    -DDEDICATED=0 -DBF2_IMAGE_RENDDX9=1 -DBF2_STATIC_PLUGINS=1 -D_ITERATOR_DEBUG_LEVEL=0 \
    -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
    -include "$ROOT/Code/BF2/Core/PlatformCompat.h" \
    -include "$ROOT/Code/BF2/Platform/Gfx/MacSupport.h" \
    -I"$ROOT/Code/BF2/Platform/Gfx/MacInclude" \
    -I"$ROOT/Code/BF2/Platform/Gfx/Include" \
    "$ROOT/Code/Tests/macos_menu_unblock/bink_policy.cpp" -o "$OUT/bink_policy"
"$OUT/bink_policy"
c++ -arch x86_64 -std=gnu++14 -fms-extensions -DDEDICATED=0 -D_MACOSX=1 \
    -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
    -include "$ROOT/Code/BF2/Core/PlatformCompat.h" \
    "$ROOT/Code/Tests/macos_menu_unblock/http_request_offline.cpp" \
    "$ROOT/Code/BF2/Game/Persistence/HTTPRequest.cpp" \
    "$ROOT/Code/BF2/Game/MatchMaking/OnlineProvider.cpp" -o "$OUT/http_request_offline"
"$OUT/http_request_offline"
# DemoLibrary on the production HTTPRequest base. Unreached
# FileManager/StringUtil dependencies stay unbound (dynamic_lookup); vtable data is linked.
Z="$ROOT/Code/BF2/External/zlib-1.2.3"
c++ -arch x86_64 -std=gnu++14 -fms-extensions -w -DDEDICATED=0 -D_MACOSX=1 -DBF2_PLATFORM_POSIX \
    -I"$Z" -I"$Z/contrib/minizip" \
    -include "$ROOT/Code/BF2/Core/BuildConfig.h" \
    -include "$ROOT/Code/BF2/Core/PlatformCompat.h" \
    "$ROOT/Code/Tests/macos_menu_unblock/demo_library_offline.cpp" \
    "$ROOT/Code/BF2/Game/DemoRecorder/DemoLibrary.cpp" \
    "$ROOT/Code/BF2/Game/Persistence/HTTPRequest.cpp" \
    "$ROOT/Code/BF2/Game/MatchMaking/OnlineProvider.cpp" \
    "$ROOT/Code/BF2/Core/ClassManager.cpp" \
    "$ROOT/Code/BF2/IO/File/FileManager.cpp" \
    "$ROOT/Code/BF2/IO/File/UnixFileSystem.cpp" \
    "$ROOT/Code/BF2/Core/StringUtil.cpp" \
    -Wl,-undefined,dynamic_lookup -o "$OUT/demo_library_offline"
"$OUT/demo_library_offline"
