# pkgctl changelog

## 0.1.0 - 2026-07-26

### Project

- Established an original C++17 Zeppe-Lin package manager under
  GPL-3.0-or-later, copyright Alexandr Savca.
- Began from an empty licensed repository without inheriting `pkgman` source,
  internal types, or transaction machinery.
- Added a separately testable internal orchestration library and a deliberately
  non-operational CLI exposing only help and version.

### Orchestration model

- Added validated package-name values and closed install, update, remove,
  system-update, and download user intents.
- Added separate execution, resolver, subtree, installed-release, and candidate
  constraint classes.
- Added native succeeded, refused, failed, and skipped step outcomes without
  backend-specific exit-code semantics.
- Added immutable package-operation DAGs with deterministic dependency-safe
  execution order.
- Rejected duplicate identities, multiple operations for one package, missing
  prerequisites, self-dependencies, and cycles.

### Documentation and qualification

- Defined the orchestration authority, external authority graph, clean-room
  boundary, target-context policy, and compatibility strategy.
- Added `pkgctl(1)` and `pkgctl_orchestration(7)` source manuals.
- Added GCC and Clang direct qualification, independent-header tests, CLI
  contracts, clean-room source checks, manual checks, and release metadata
  checks.

### Deliberate boundary

Version 0.1.0 performs no source inspection, building, archive opening,
planning, filesystem application, canonical state publication, lifecycle
execution, or recovery. Transaction commands fail explicitly until the exact
authority adapters are implemented and qualified.
