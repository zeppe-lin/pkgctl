# Contributing to pkgctl

Contributions must preserve the clean-room and authority boundaries documented
in `DESIGN.md`.

Do not submit code copied or mechanically translated from `pkgman`, CRUX
`prt-get`, or another package manager. Do not reproduce their internal type
systems merely under new names.

`pkgctl` must not define alternate package references, profile semantics,
resolver constraints, selected-package records, transaction graphs, package
plans, lifecycle programs, application evidence, or installed-state records.
Use the exact owning library values and retain their identities through the
controller session and effect result.

Changes should be small, contract-first commits. Every implementation commit
must compile and its applicable tests must pass. New authority calls require
exact dependency bundles, boundary tests, failure tests, and documentation of
what remains deliberately unavailable. Effectful
changes must prove that no state is published before all required subordinate
evidence is complete and that lease loss is never hidden. Durable changes must
write intent before effects, retain exact subordinate evidence afterward, and
stop rather than guess when restart authority is incomplete. Construction
changes must bind one exact transaction node and retain subordinate fetch/build
evidence without interpreting it. Preparation changes must compose the official
state, source/build, planner, and application adapters; typed planner refusal
must never be converted into a partial effect request. Check changes must keep
pure transaction admission separate from concrete host resources, delegate
execution semantics to `libpkgcheck` and `libpkgcheck-exec`, and refuse foreign
or stale terminal evidence before progression. Dispatch changes must keep
reservation separate from execution admission, retain exact predecessor and
attempt identities, forbid duplicate ownership, and never release started work
as if it were unstarted. Failure containment must stop new work without erasing
terminal evidence from already-started independent work.

Use SPDX headers:

```text
SPDX-FileCopyrightText: 2026 Alexandr Savca
SPDX-License-Identifier: GPL-3.0-or-later
```

C++ code targets C++17. Warnings are errors in qualification builds.
