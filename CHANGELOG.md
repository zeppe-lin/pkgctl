# pkgctl changelog

## 0.6.0 - 2026-07-29

### One-operation preparation

- Added one exact preparation request for transaction install, upgrade, and
  removal nodes.
- Required incoming operations to bind one completed construction result to the
  exact transaction build node and `build_before_target` edge.
- Projected canonical installed truth through `libpkgstate-plan` and incoming
  candidate, image, and artifact truth through `libpkgbuild-plan`.
- Reinspected exact artifact bytes through an injected backend and required the
  archive digest, normalized image identity, and entry count to reproduce
  construction evidence without requiring one inspection backend identity.
- Admitted the exact incoming package authority through `libpkgapply` before
  package-local planning.

### Planning and request sealing

- Called the operation-specific `libpkgplan` install, upgrade, or removal
  planner using caller-supplied observations, runtime closure, normalized
  package policy, target context, and execution guarantees.
- Retained typed planning refusal as a terminal preparation result with no
  partial package plan, application request, or effect request.
- Sealed successful plans into exact `libpkgapply` requests and the existing
  one-operation effect request, including explicit lifecycle order and install
  reason authority.
- Kept removal independent of construction and incoming artifact projection.

### Deliberate boundary

- Preparation may inspect retained artifact bytes but performs no target
  observation, target lease acquisition, lifecycle execution, application, or
  state publication.
- Added no recursive construction, check execution, cross-package scheduler,
  durable preparation journal, Linux-backend policy, or effectful CLI command.
- Retained the construction, durable effect, and restart contracts from 0.5.0
  and 0.4.0 unchanged.

### Authority floors

- `libpkgsource >= 1.1.0` and `libpkgsource-plan >= 1.1.0`;
- `libpkgcatalog >= 1.1.0` and `libpkgcatalog-acquire >= 1.1.0`;
- `libpkgstate >= 2.2.0`, `libpkgstate-plan >= 2.2.0`, and
  `libpkgstate-apply >= 2.2.0`;
- `libpkgfetch >= 0.1.0`;
- `libpkgbuild >= 1.0.0`, `libpkgbuild-exec >= 0.1.0`, and
  `libpkgbuild-plan >= 1.0.0`;
- `libpkgimage >= 0.3.0`;
- `libpkgplan >= 0.2.0`;
- `libpkgexec >= 1.2.0`;
- `libpkgapply >= 1.0.0` and `libpkgapply-exec >= 0.1.0`;
- `libpkgresolve >= 1.0.0`;
- `libpkgtransaction >= 1.1.0`.

## 0.5.0 - 2026-07-29

### Candidate construction sessions

- Added one exact controller construction request for a catalog-backed
  transaction build node.
- Bound the transaction, source snapshot, canonical package-input set, selected
  architectures, build policy, and bounded acquisition policy into controller
  identity. Package inputs must match the resolver's exact required release and
  source authority.
- Added an admitted call-scoped session carrying explicit source/store roots,
  dependency-tree paths, root view, workspace/output/artifact paths, interpreter,
  credentials, and compression.
- Added an injected construction driver and native composition of `libpkgfetch`
  with `libpkgbuild-exec`.
- Retained complete verified source materialization, execution evidence, build
  result, artifact binding, and independent archive-inspection evidence.
- Promoted completion only for successful builds with complete artifact evidence;
  failed executions remain failed builds and do not publish artifacts.

### Authority and compatibility

- Kept all host paths, cache/transport observations, and timing outside semantic
  construction identity.
- Kept the construction layer backend-neutral and free of `libpkgexec-linux`.
- Added no recursive dependency scheduler, check-node execution, target plan,
  state publication, durable construction journal, or effectful CLI command.
- Retained the durable target-effect and restart contracts from 0.4.0 unchanged.

### Authority floors

- `libpkgsource >= 1.1.0`;
- `libpkgcatalog >= 1.1.0` and `libpkgcatalog-acquire >= 1.1.0`;
- `libpkgstate >= 2.2.0` and `libpkgstate-apply >= 2.2.0`;
- `libpkgfetch >= 0.1.0`;
- `libpkgbuild >= 1.0.0` and `libpkgbuild-exec >= 0.1.0`;
- `libpkgimage >= 0.3.0`;
- `libpkgexec >= 1.2.0`;
- `libpkgapply >= 1.0.0` and `libpkgapply-exec >= 0.1.0`;
- `libpkgresolve >= 1.0.0`;
- `libpkgtransaction >= 1.1.0`.

## 0.4.0 - 2026-07-29

### Durable effect attempts

- Added caller-nonced, append-only controller attempt snapshots for one exact
  effectful operation session.
- Recorded durable intent before every lifecycle, application, and publication
  handoff and exact terminal evidence after each completed handoff.
- Added a strict checksummed binary codec with bounded decoding, predecessor
  linkage, and causal-shape validation.
- Added an FD-anchored POSIX journal store with immutable non-replacing
  publication, record and directory synchronization, and filename/content
  identity validation.

### Restart and reconciliation

- Added pure restart classification and exact restart checkpoints retaining all
  subordinate lifecycle, application, publication, and controller evidence.
- Required external resolution for lifecycle intent without terminal execution
  evidence.
- Required the exact durable `libpkgapply` journal to resume an interrupted
  application handoff.
- Required a newly held target-mutation lease for every resumed attempt.
- Retried publication only after observing the exact expected prior state.
- Reconciled an interrupted publication from the exact resulting installed
  state without publishing twice.
- Refused contradictory installed state, stale records, missing subordinate
  authority, and mismatched evidence instead of guessing.

### Authority floors

The authority floors are unchanged from 0.3.0:

- `libpkgsource >= 1.1.0`;
- `libpkgcatalog >= 1.1.0` and `libpkgcatalog-acquire >= 1.1.0`;
- `libpkgstate >= 2.2.0` and `libpkgstate-apply >= 2.2.0`;
- `libpkgimage >= 0.3.0`;
- `libpkgexec >= 1.2.0`;
- `libpkgapply >= 1.0.0` and `libpkgapply-exec >= 0.1.0`;
- `libpkgresolve >= 1.0.0`;
- `libpkgtransaction >= 1.1.0`.

### Deliberate boundary

- Preserved the semantic effect-session identity and the ordinary 0.3 result
  identity path; reconciled results additionally bind the observed state.
- Kept the executable read-only with only `catalog`, `resolve`, and
  `transaction` commands.
- Did not add cross-package scheduling, source construction, build execution,
  effectful CLI policy, lifecycle replay assumptions, or application-journal
  discovery.

## 0.3.0 - 2026-07-28

### Effectful operation sessions

- Added one exact effectful controller session for a single install, upgrade, or
  removal transaction node.
- Bound the selected transaction node, exact `libpkgapply` request, caller-owned
  lifecycle order, and admitted `libpkgapply-exec` sessions into controller
  request and session identities.
- Added an injected physical driver surface for lifecycle execution,
  application, and installed-state publication.
- Retained complete subordinate lifecycle, application, transaction, and
  publication evidence in one terminal controller result.

### Sequencing and publication

- Required one outer target-mutation lease across pre-action lifecycle,
  filesystem application, post-action lifecycle, and state publication.
- Rechecked the lease after state publication so a completed backend receipt
  cannot hide loss of the controller's exclusion authority.
- Added exact install, upgrade, and removal sequencing, including historical
  removal and incoming installation lifecycle nodes around one upgrade action.
- Published installed state only after all required lifecycle and application
  evidence completed successfully.
- Carried one exact transaction-evidence identity into `libpkgstate` publication
  and durable installation or upgrade receipts.
- Distinguished lifecycle failure, incomplete application, lease loss,
  non-completed publication, indeterminate publication, and completed outcome
  without claiming rollback of arbitrary lifecycle side effects.

### Authority floors

- `libpkgsource >= 1.1.0`;
- `libpkgcatalog >= 1.1.0` and `libpkgcatalog-acquire >= 1.1.0`;
- `libpkgstate >= 2.2.0` and `libpkgstate-apply >= 2.2.0`;
- `libpkgimage >= 0.3.0`;
- `libpkgexec >= 1.2.0`;
- `libpkgapply >= 1.0.0` and `libpkgapply-exec >= 0.1.0`;
- `libpkgresolve >= 1.0.0`;
- `libpkgtransaction >= 1.1.0`.

### Deliberate boundary

- The executable remains read-only and still exposes only `catalog`, `resolve`,
  and `transaction`.
- The effectful API accepts one already-planned, already-authorized package
  operation; it does not fetch sources, build artifacts, derive plans, schedule
  multiple packages, or choose recovery policy.
- Transaction-wide filesystem/state atomicity and rollback of lifecycle effects
  are not claimed.

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
