# Third-party notices

## Interest Engine kernel (vendored, unmodified)

`ThirdParty/InterestEngine/` contains four unchanged kernel code files and the license that
applies to them, copied from [PullsarDev/InterestEngine](https://github.com/PullsarDev/InterestEngine).

- Release tag: `v0.2.0` (peeled commit below).
- Commit: `52e33c42be2445ef95a5024e47b570153279a7a6`.
- Files: `include/interest/attention.hpp`, `include/interest/version.hpp`,
  `src/step.cpp`, `src/version.cpp`, `LICENSE`.
- License: Apache License 2.0 (`ThirdParty/InterestEngine/LICENSE`, verbatim).
  Each code file keeps its original header, `SPDX-FileCopyrightText: 2026 PullsarDev` and
  `SPDX-License-Identifier: Apache-2.0`.
- Local modifications: none. These files are the kernel's own work and are listed here only to
  record their provenance; vendoring them does not change who holds their copyright.

Git blob hashes, verified against that upstream commit:

| File | Git blob SHA-1 |
| --- | --- |
| `include/interest/attention.hpp` | `3dd6a3cf8edaa534c57bee7615fa788dc8b82d6b` |
| `include/interest/version.hpp` | `6de3eb11589771d4360b342a556728b970c46424` |
| `src/step.cpp` | `e49ac324d7c80698055e098db3d3de901821ff28` |
| `src/version.cpp` | `6c6874a96e3da801c0933070f06777eb0a72dca6` |
| `LICENSE` | `d645695673349e3947e8e5ae42332d0ac3164cd7` |

Re-check them with `git hash-object <file>` (the files carry `-text` in `.gitattributes` so line
endings are never normalized). The upstream test and benchmark code, including its SplitMix64
implementation, is not included. The upstream repository has no separate `NOTICE` file at this
revision.

## Unreal Engine

Unreal Engine is not distributed here. The adapter is compiled against the engine's public
headers by the engine's own build tools, and use of the engine is governed by its own license.
Unreal Engine is a trademark of Epic Games, Inc. This project is not affiliated with or endorsed
by Epic Games.

## This repository's own code

The adapter (`Source/`), the validation script and consumer test (`Tests/`) and the
documentation are licensed under the Apache License 2.0; see [LICENSE](LICENSE). Copyright 2026
PullsarDev. Source files carry SPDX identifiers.
