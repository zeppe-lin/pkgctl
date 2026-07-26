# Contributing to pkgctl

Contributions must preserve the clean-room and authority boundaries documented
in `DESIGN.md`.

Do not submit code copied or mechanically translated from `pkgman`, CRUX
`prt-get`, or another package manager. Do not reproduce their internal type
systems merely under new names.

Changes should be small, contract-first commits. Each commit should compile and
its tests should pass. New orchestration concepts require documented invariants
and direct regression coverage before effectful adapters use them.

Use SPDX headers:

```text
SPDX-FileCopyrightText: 2026 Alexandr Savca
SPDX-License-Identifier: GPL-3.0-or-later
```

C++ code targets C++17. Warnings are errors in qualification builds.
