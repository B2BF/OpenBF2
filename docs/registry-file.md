# Client registry file

`[CHOSEN, not recovered]` Retail BF2 reads its CD keys and language from the Windows registry
(`HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\<product>`). Writing there takes
administrator rights. OpenBF2 clients also read a per-user file, so an installer can supply
the keys without elevation. The Windows installer has two modes: per-user (the default, no
administrator rights, this file) and all-users (administrator rights, the registry; see
[All-users installs](#all-users-installs)). The client only reads this file and never creates or changes it.

| Platform | File | Protection | Contract |
| --- | --- | --- | --- |
| macOS (and future POSIX clients) | `<support root>/Registry.con` | none (mode 0600) | [macOS write paths](macos-write-paths.md#registrycon-syntax) |
| Windows | `%LOCALAPPDATA%\OpenBF2\Registry.dat` | DPAPI, current user | this page |

Both platforms parse the text with the same code (`Code/BF2/Core/Registry.cpp`, declared in
`Core/RegistryFile.h`). The dedicated server reads neither file.

## Windows: `Registry.dat`

The Windows installer writes this file. It is a separate project, and this section is its
contract.

- **Path.** `<LocalAppData>\OpenBF2\Registry.dat`, where `<LocalAppData>` is the known folder
  `FOLDERID_LocalAppData` of the user who runs the game (normally the same as
  `%LOCALAPPDATA%`, for example `C:\Users\<name>\AppData\Local`). The installer creates the
  `OpenBF2` folder when it does not exist. The default per-user ACL is enough.
- **Plaintext.** The [`Registry.con` syntax](macos-write-paths.md#registrycon-syntax), one
  `HIVE\registry\path\value-name = value` line per value, encoded as UTF-8. Keep keys and values
  ASCII; value bytes reach the game unchanged. Write it without a byte-order mark (one leading
  UTF-8 BOM is skipped). Use CRLF or LF line endings. Trailing NUL bytes are ignored, so
  protecting a C string together with its terminator also works. Use the plain CD key text,
  not a DPAPI-protected `x9392` value. Example with fake keys:

  ```text
  HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2\ergc = FAKE-0000-1111-2222-3333
  HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2 Special Forces\ergc = FAKE-XPCK-4444-5555-6666
  ```

- **Protection.** `CryptProtectData` over exactly those bytes, as the user who will run the game:
  - flags `CRYPTPROTECT_UI_FORBIDDEN`, without `CRYPTPROTECT_LOCAL_MACHINE` (current-user scope);
  - no optional entropy (`pOptionalEntropy = NULL`). A blob protected with entropy does not
    decrypt and is treated as unreadable;
  - any description string (the client ignores it).

  .NET's `ProtectedData.Protect(bytes, null, DataProtectionScope.CurrentUser)` produces the same
  blob. An installer that runs elevated must protect and write as the target user, not as the
  administrator account. The client cannot decrypt another user's blob.
- **File contents.** The raw `DATA_BLOB` bytes, with no header and no encoding. The file can
  be at most 64 KiB.
- **Reading.** For every registry lookup (`Registry::getString`: the CD key `ergc` for the base
  game and for each expansion, and `Language`) the client:
  1. reads the file and decrypts it (`CryptUnprotectData`, `CRYPTPROTECT_UI_FORBIDDEN`, no
     entropy), then looks up the named value
     `HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\<product>\<name>`, and then the
     default value `...\<product>\<name>\@`. The first one found is the result;
  2. otherwise reads the registry exactly as retail does (on x64 the 32-bit `WOW6432Node`
     view), including retail's default when the registry has no value either.

  A missing file is silent. If the file is present but cannot be read or decrypted, the client
  writes one line without values (`Registry.dat: unreadable, using the registry`) and uses the
  registry. A value missing from the file is not an error. For example, a file with only the CD
  keys still takes `Language` from the registry. Nothing is cached, so changes apply to the
  next lookup. Diagnostics never contain values.
- **Removal.** Deleting the file restores the retail registry behaviour. Uninstallers should
  remove the file and the `OpenBF2` folder only when it is empty.

### All-users installs

An all-users install (administrator rights) writes no file. It puts the keys into the
registry, laid out as retail writes them, and every account on the machine can read them:

- the CD key is the `REG_SZ` value `ergc` of
  `HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\<product>` (`Battlefield 2`,
  `Battlefield 2 Special Forces`). Use the 32-bit registry view, `WOW6432Node` on 64-bit
  Windows, which both clients read;
- the value is either the plain key, or `x9392` followed by the lowercase hex of a DPAPI
  blob. The blob is `CryptProtectData` over the key text **including its terminating NUL**,
  with `CRYPTPROTECT_LOCAL_MACHINE | CRYPTPROTECT_UI_FORBIDDEN` and no entropy, so every
  account can decrypt it. The existing CD-key path (`Game/MatchMaking/Client/CdKeyStore.cpp`,
  `translateCdKey`) strips the marker and decrypts the blob as retail does.

A `Registry.dat` value of the user who runs the game takes precedence over these values.

### Testing

`tools/windows/new-registry-dat.ps1` writes a test file with the same protection. Use fake
keys only:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\windows\new-registry-dat.ps1 -CdKey FAKE-0000-1111-2222-3333
```

`-XpackCdKey` adds the Special Forces key. `-InputFile` protects ready-made `Registry.con`
text, and `-Path` writes to a location other than the client's. The equivalent one-liner is:

```powershell
Add-Type -AssemblyName System.Security; $t = 'HKEY_LOCAL_MACHINE\SOFTWARE\Electronic Arts\EA Games\Battlefield 2\ergc = FAKE-0000-1111-2222-3333'; New-Item -ItemType Directory -Force "$env:LOCALAPPDATA\OpenBF2" | Out-Null; [IO.File]::WriteAllBytes("$env:LOCALAPPDATA\OpenBF2\Registry.dat", [Security.Cryptography.ProtectedData]::Protect([Text.Encoding]::ASCII.GetBytes($t), $null, 'CurrentUser'))
```

The MSVC check `bf2_registry_dat_check` (`Code/Tests/registry_dat_check.cpp`, part of
`bf2_run_checks`) round-trips fake keys through `CryptProtectData` and the production reader.
`bf2_registry_dat_script` runs the script and reads its output with the same check.
