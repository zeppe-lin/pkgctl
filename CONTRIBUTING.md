# Contributing to pkgctl

Contributions must preserve the clean-room and authority boundaries documented
in `DESIGN.md`.

Do not submit code copied or mechanically translated from `pkgman`, CRUX
`prt-get`, or another package manager. Do not reproduce their internal type
systems merely under new names.

`pkgctl` must not define alternate package references, profile semantics,
resolver constraints, selected-package records, transaction graphs, package
plans, application evidence, or installed-state records. Use the exact owning
library values and retain their identities through the controller session.

Changes should be small, contract-first commits. Every implementation commit
must compile and its applicable tests must pass. New authority calls require
exact dependency bundles, boundary tests, failure tests, and documentation of
what remains deliberately unavailable.

Use SPDX headers:

```text
SPDX-FileCopyrightText: 2026 Alexandr Savca
SPDX-License-Identifier: GPL-3.0-or-later
```

C++ code targets C++17. Warnings are errors in qualification builds.
