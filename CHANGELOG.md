# pkgctl changelog

## 0.2.0 - 2026-07-27

### Native control loop

- Replaced the provisional controller-owned package, intent, constraint,
  outcome, and operation-graph models with exact native authority handoffs.
- Added explicit catalog, state, resolution, and transaction controller
  requests.
- Added read-only catalog, resolution, and transaction sessions with
  domain-separated session identities.
- Added deterministic line-oriented reports retaining exact authority
  identities and normalized summaries.

### Commands

- Added `pkgctl catalog` for explicit native collection acquisition.
- Added `pkgctl resolve` for one exact catalog plus installed-state resolution.
- Added `pkgctl transaction` for immutable cross-package program composition.
- Added typed `--goal SCOPE=SUBJECT`, explicit architecture and target-binding
  options, `--prefer-catalog`, and opt-in `--converge-exact`.
- Kept destructive convergence disabled by default.

### Authority floors

- `libpkgsource >= 1.1.0`;
- `libpkgcatalog >= 1.1.0` and `libpkgcatalog-acquire >= 1.1.0`;
- `libpkgstate >= 2.1.0`;
- `libpkgresolve >= 1.0.0`;
- `libpkgtransaction >= 1.0.0`.

### Deliberate boundary

- The release is read-only and never initializes, mutates, repairs, or publishes
  installed state.
- Build, check, artifact inspection, package-local planning, lifecycle
  execution, application, recovery, and publication remain unavailable.
- Effect-implying `install`, `update`, `remove`, `system-update`, and `download`
  commands are not exposed.

## 0.1.0 - 2026-07-26

### Project

- Established the clean-room C++17 repository and GPL-3.0-or-later license.
- Added provisional orchestration values and a help/version-only executable.

### Superseded model

The provisional intent, constraint, step-outcome, and operation-DAG values were
removed in 0.2.0 after their semantics became authoritative in
`libpkgresolve` and `libpkgtransaction`.
