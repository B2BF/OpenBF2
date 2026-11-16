# gameswf XML fixture

[TEMP] [CHOSEN, not recovered] Native integration check using the production
gameswf XML object, String implementation, file reader and Expat substitute.
No game content, GPU or network is used. The macOS target supplies test-only
ClientHost symbols for links that also pull game UI natives from the SwiffPlayer
archive. Each symbol reports a qualified `[FRONTIER]`, flushes stderr
and aborts if called; a passing XML run cannot exercise cursor or keyboard input.
Production host and SwiffPlayer sources are unchanged. Remove this scaffolding
when the fixture's link no longer pulls those natives.

In an already configured macOS client build (enable CTest registration):

```sh
cmake -S . -B out/build/swf-s3-expat3 -DBUILD_TESTING=ON
cmake --build out/build/swf-s3-expat3 -j4 --target bf2_gameswf_xml_check
ctest --test-dir out/build/swf-s3-expat3 -R '^bf2_gameswf_xml_check$' --output-on-failure
```

CTest supplies a scratch filename in that build directory. The fixture writes
and removes only that file. It checks a tiny string and file, element/text/attribute
links, mismatched tags, non-final incomplete input, status mapping, String onData
payload/receiver, untouched load result and undefined onData on open failure.
Parse errors still deliver a defined source String to onData.
This establishes native XML integration only; it does not publish the XML
factories in the VM or establish menu/gameplay acceptance or parser equivalence.

The source is C++14; MSVC x86/x64 execution remains a maintainer gate.
