// Typed host file interfaces: the engine's declarations, shared with the host (issue #7).

#ifndef RENDDX9ABI_HOSTFILEABI_H
#define RENDDX9ABI_HOSTFILEABI_H

#include "HfeAbi.h"


#include "../BF2/IO/File/IFileSystem.h"

namespace renddx9abi {
typedef ::dice::hfe::io::IStream          HostStream;
typedef ::dice::hfe::io::INonlinearStream HostNonlinearStream;
typedef ::dice::hfe::io::IFileManager     HostFileManager;
typedef ::dice::hfe::io::FileMode         HostFileMode;
typedef ::dice::hfe::io::FileAccess       HostFileAccess;
typedef ::dice::hfe::io::SeekOrigin       HostSeekOrigin;
}


#endif // RENDDX9ABI_HOSTFILEABI_H
