# Engine documentation

Start with [Getting started](getting-started.md), then read [Engine architecture](architecture.md)
and [Development guide](development.md). These guides cover the build, engine structure,
contribution workflow and validation requirements.

## Guides

| Topic | Guide |
| --- | --- |
| First build and runtime setup | [Getting started](getting-started.md) |
| Toolchains, targets, and build checks | [Building](building.md) |
| Engine images, source layout and lifecycle | [Engine architecture](architecture.md) |
| Recovery prerequisites, contribution rules and validation | [Development guide](development.md) |
| Editor console commands and script undo | [Editor console guide](editor-console.md) |
| Server browser filter mask and menu ActionScript tool | [Server browser filters](server-browser-filters.md) |

[AGENTS.md](../AGENTS.md) is the working policy. Third-party documentation stays with its
dependency under `Code/BF2/External/`. The validation commands are in the development guide,
with harnesses in `Code/Tests/` and `Code/Build/`. Compilation, a successful link and a
runtime milestone are different results; check the paths a test exercised before drawing
conclusions.

## Where to put new information

Keep stable explanations of how to build, navigate, and change the engine in these guides.
Keep function and class comments focused on ownership, ordering, ABI constraints, surprising
behavior, and unresolved work. Public documentation must state the contract or limitation
directly. Describe the implementation and test conditions required to remove a temporary
boundary beside its source marker. Keep guides focused on how the engine works and how to
change it safely.
