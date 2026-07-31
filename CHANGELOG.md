# pkgctl changelog

## 0.12.0 - 2026-07-31

### Durable one-dispatch restart reconciliation

- Adds explicit durable reconciliation for one dispatch selected by an exact
  `transaction_run_restart_checkpoint`; it does not select or reserve work.
- Durably releases only a reservation classified as never started.
- Accepts caller-rehydrated construction and check results only when their
  admitted sessions equal the attempt session retained by started ownership.
- Commits recovered build and check completion through the existing pure
  dispatch transitions and the common exact run-successor commit boundary.
- Binds operation recovery to both the effect attempt retained by the run and
  the latest exact record supplied by the effect journal store.
- Delegates automatically continuable operation stages to the existing effect
  restart authority and repairs a terminal effect journal whose final run append
  was lost without invoking target mutation again.
- Leaves the committed run unchanged, invokes no driver, and appends no journal
  record when effect restart requires external resolution.
- Requires a fresh canonical installed-state read before a resumed successful
  operation can advance transaction progression.
- Makes every failed terminal run append exactly retryable from the previous
  committed restart checkpoint.
- Fixes terminal effect-result rehydration to snapshot transaction evidence
  before moving the retained publication request, eliminating dependence on C++
  function-argument evaluation order and preserving exact result identity.
- Adds no scheduler, execution loop, evidence or resource discovery, process
  adoption, retry timing, rollback, compaction, garbage collection, or
  effect-implying CLI command.
- Retains the complete authority floors established by Version 0.10.0.

## 0.11.0 - 2026-07-31

### Durable single-dispatch execution

- Adds `commit_transaction_run_successor()` as the common exact append and
  returned-authority verification boundary for legal run transitions.
- Adds explicit one-dispatch construction, check, and operation execution APIs;
  callers supply the selected reservation, admitted session, injected driver,
  and exact stores.
- Commits construction and check started ownership before driver invocation,
  then commits only terminal evidence accepted by the existing dispatch and
  progression authorities.
- Preserves the operation write-ahead order: effect-attempt admission, started
  run ownership, durable effect execution, authoritative state reread after
  successful publication, and final run successor.
- Leaves lost-lease and indeterminate-publication results as retained
  observations on active operation ownership.
- Makes start-store failure a hard no-driver boundary and leaves exact started
  ownership durable after driver escape or final-store failure.
- Keeps a terminal effect journal reconcilable when the corresponding final run
  append is lost.
- Adds failure-injection tests for effect admission, run start, driver escape,
  terminal commitment, uncertainty retention, and restart classification.
- Adds no reservation loop, scheduler, thread creation, backend construction,
  resource discovery, automatic retry, automatic release, rollback, or
  effect-implying CLI command.
- Retains the complete authority floors established by Version 0.10.0.

## 0.10.0 - 2026-07-30

### Durable transaction-run ownership

- Adds caller-nonced `transaction_run_journal_record` snapshots admitted before
  the first dispatch reservation and advanced by exactly one legal ledger
  transition.
- Binds every snapshot to the exact transaction, dispatch policy, predecessor,
  sequence, run, progression, canonical state epoch, terminal flags, and
  immutable dispatch-record identities.
- Reopens dispatch ownership only after the caller rehydrates the exact
  `transaction_progress`; stored identities never fabricate construction,
  check, effect, or installed-state evidence.
- Revalidates durable graph units, predecessor evidence, completed attempt and
  terminal evidence, and active operation state epochs before rebuilding a run.
- Adds conservative restart assessment: release a still-reserved unit, recover
  a started construction or check from external evidence, or inspect the exact
  effect-attempt history retained by a started operation.
- Binds every started operation dispatch to its exact admitted effect attempt,
  and preserves that identity through uncertainty observations and terminal
  completion.

### Cross-journal operation start

- Adds `commit_operation_dispatch_start()`, which commits the exact
  effect-attempt admission before committing the started run snapshot and
  invokes no effect driver.
- Verifies the authority returned by both abstract stores and reports a typed
  store-contract violation rather than accepting foreign committed records.
- Makes an interrupted cross-journal start exactly retryable with the same
  transaction run, dispatch, session, and effect-attempt nonce.
- Treats a committed effect admission with an uncommitted started run as an
  orphan admission: restart still sees a releasable reservation and never
  infers that target mutation began.
- Treats a committed started run with a missing effect journal as unresolved
  ownership requiring exact effect-journal inspection; no terminal result is
  fabricated.

### Crash-consistent run store

- Adds a bounded deterministic binary run-journal codec and an explicit POSIX
  store rooted at a caller-selected directory or directory descriptor.
- Publishes immutable read-only snapshots without replacement, then atomically
  advances a checksummed read-only head as the physical commit point.
- Loads only the exact self-contained head-selected snapshot; missing, corrupt,
  writable, symlinked, or contradictory heads and snapshots fail closed.
- Makes exact retries idempotent before and after head publication and across
  concurrent processes, while requiring byte-for-byte equality for an already
  published snapshot.
- Documents that the store is crash-consistent but not an anti-rollback trust
  anchor, and that complete snapshots are bounded to 16 MiB with no discovery,
  compaction, or garbage collection in this release.
- Adds no semantic-evidence serialization, resource recovery, automatic driver
  execution, retry policy, rollback, or effect-implying CLI command.

### Authority floors

- `libpkgsource >= 2.0.0`, `libpkgsource-yaml >= 2.0.0`, and
  `libpkgsource-plan >= 2.0.0`;
- `libpkgcatalog >= 2.0.0` and `libpkgcatalog-acquire >= 2.0.0`;
- `libpkgstate >= 2.3.0`, `libpkgstate-plan >= 2.3.0`, and
  `libpkgstate-apply >= 2.3.0`;
- `libpkgfetch >= 1.0.0`;
- `libpkgbuild >= 2.0.0`, `libpkgbuild-exec >= 1.0.0`, and
  `libpkgbuild-plan >= 2.0.0`;
- `libpkgimage >= 0.3.0` and `libpkgplan >= 0.2.0`;
- `libpkgexec >= 1.3.0`;
- `libpkgapply >= 2.0.0` and `libpkgapply-exec >= 1.0.0`;
- `libpkgresolve >= 2.0.0` and `libpkgtransaction >= 2.1.0`;
- `libpkgcheck >= 0.1.0` and `libpkgcheck-exec >= 0.1.1`.

## 0.9.1 - 2026-07-30

### Effect-journal commit-point hardening

- Introduces effect-attempt encoding version two and an explicit checksummed
  read-only head file as the physical commit point for the POSIX store.
- Publishes immutable read-only snapshots without replacement, synchronizes the
  snapshot and directory, atomically replaces the head, and synchronizes the
  directory again before append reports success.
- Loads only the exact self-contained snapshot selected by the committed head;
  missing, corrupt, writable, symlinked, or contradictory heads and snapshots
  fail closed.
- Makes exact append retries idempotent before and after head publication and
  requires byte-for-byte equality when a snapshot name already exists.
- Keeps strict version-one record-only histories readable through full-chain
  validation and upgrades them on the first exact version-two append.
- Tightens bounded encoder/decoder failures, no-follow/type/mode checks, I/O
  error mapping, locking, and directory-descriptor operation.
- Documents that the store is crash-consistent but is not an anti-rollback trust
  anchor; historical snapshots remain audit material rather than an implicit
  newest-record index.
- Adds no dispatch persistence, semantic-evidence reconstruction, automatic
  execution, retry policy, rollback, or effect-implying command.

## 0.9.0 - 2026-07-30

### Transaction dispatch and ownership

- Adds immutable `transaction_run`, `transaction_dispatch`, and lifetime record
  values over one exact transaction progression.
- Adds deterministic `reserve_next()` selection in canonical ready-unit order,
  with explicit construction/check capacities and one invariant operation lane.
- Binds every physical dispatch attempt to a caller-issued nonzero 32-byte nonce
  that cannot be reused within the run, including after release or completion.
- Separates `reserved`, `started`, `completed`, and `released_unstarted` states;
  only work that never acquired execution authority may be released.
- Retains exact terminal predecessor evidence at reservation and refuses units
  whose graph readiness or predecessor authority changes before admission.
- Validates construction package inputs against exact transaction requirement
  edges, selected or retained package authority, successful predecessor build
  results, artifacts, and current installed truth.
- Requires `libpkgtransaction >= 2.1.0`, which orders check-scoped package inputs
  before the checked package construction that must seal them.
- Binds check dispatches to the exact construction retained by progression and
  operation dispatches to the exact action, effect session, and reserved state
  epoch.
- Delegates terminal knowledge to existing `advance_construction()`,
  `advance_check()`, and `advance_effect()` functions rather than reinterpreting
  subordinate outcomes.
- Retains lost-lease and indeterminate-publication operation results as active,
  ordered uncertainty observations; such observations cannot carry resulting
  state.
- Stops new reservations and starting merely reserved work after terminal
  failure while accepting exact terminal evidence from independent work already
  started.
- Adds no automatic execution, backend creation, retry, adaptive scheduler,
  durable transaction-run store, rollback, or effect-implying CLI command.

### Authority floors

- `libpkgsource >= 2.0.0`, `libpkgsource-yaml >= 2.0.0`, and
  `libpkgsource-plan >= 2.0.0`;
- `libpkgcatalog >= 2.0.0` and `libpkgcatalog-acquire >= 2.0.0`;
- `libpkgstate >= 2.3.0`, `libpkgstate-plan >= 2.3.0`, and
  `libpkgstate-apply >= 2.3.0`;
- `libpkgfetch >= 1.0.0`;
- `libpkgbuild >= 2.0.0`, `libpkgbuild-exec >= 1.0.0`, and
  `libpkgbuild-plan >= 2.0.0`;
- `libpkgimage >= 0.3.0` and `libpkgplan >= 0.2.0`;
- `libpkgexec >= 1.3.0`;
- `libpkgapply >= 2.0.0` and `libpkgapply-exec >= 1.0.0`;
- `libpkgresolve >= 2.0.0` and `libpkgtransaction >= 2.1.0`;
- `libpkgcheck >= 0.1.0` and `libpkgcheck-exec >= 0.1.1`.

## 0.8.0 - 2026-07-30

- Adds pure `transaction_check_request` admission for one exact ready check node
  and its retained successful construction evidence.
- Separates concrete host resource admission into `transaction_check_session`,
  preserving the request/session boundary used by candidate construction.
- Delegates exact source, built-package, multi-input, root-view, temporary,
  interpreter, credential, and resource-limit admission to
  `libpkgcheck-exec >= 0.1.1`.
- Adds injected check execution with exact check-request and execution-request
  driver-contract validation.
- Adds `advance_check()` terminal progression: passed evidence satisfies the
  exact node; failed evidence fails it and blocks dependent graph units.
- Accepts completion after unrelated progression advancement only while the
  exact check node remains ready and its retained construction authority is
  unchanged.
- Rejects missing, duplicate, forged, aliased, cross-transaction,
  stale-construction, duplicate-completion, foreign-request, and throwing-driver
  evidence.
- Retains deterministic controller identities for pure requests, concrete
  sessions, terminal results, and rebuilt transaction progress.
- Adds direct dependencies on `libpkgcheck >= 0.1.0` and
  `libpkgcheck-exec >= 0.1.1`.
- Exposes no new CLI command and adds no ready-peer selection, automatic
  execution, retry, rollback, or durable check-session policy.

## 0.7.1 - 2026-07-29

Authority-closure migration release.

- Raised every source-derived dependency floor to the generation-2 ABI
  closure.
- Requires `libpkgsource >= 2.0.0`, `libpkgsource-yaml >= 2.0.0`, and
  `libpkgsource-plan >= 2.0.0`;
  `libpkgcatalog >= 2.0.0` and `libpkgcatalog-acquire >= 2.0.0`;
  `libpkgstate >= 2.3.0`, `libpkgstate-plan >= 2.3.0`, and
  `libpkgstate-apply >= 2.3.0`; `libpkgfetch >= 1.0.0`;
  `libpkgbuild >= 2.0.0`, `libpkgbuild-exec >= 1.0.0`, and
  `libpkgbuild-plan >= 2.0.0`; `libpkgimage >= 0.3.0`;
  `libpkgplan >= 0.2.0`; `libpkgexec >= 1.3.0`;
  `libpkgapply >= 2.0.0` and `libpkgapply-exec >= 1.0.0`;
  `libpkgresolve >= 2.0.0`; and `libpkgtransaction >= 2.0.0`.
- Adds no controller semantics, scheduler policy, check completion, or
  effectful CLI command.
- Preserves every 0.7.0 progression, preparation, effect, restart, and journal
  identity domain.

## 0.7.0 - 2026-07-29

### Transaction progression

- Added immutable transaction progression anchored to one sealed transaction
  session, one current canonical state epoch, and exact accepted terminal
  construction and effect evidence.
- Collapsed each target action and its exact pre/post lifecycle phase nodes into
  one ready operation unit while retaining per-node status and evidence.
- Treated exact retain nodes as initially satisfied and derived pending, ready,
  satisfied, failed, and blocked nodes only from the transaction graph and
  retained terminal evidence.
- Exposed every ready construction, check, and operation unit without selecting
  among ready peers. Check units can become ready but have no completion API
  until a separate check authority exists.
- Refused out-of-order, duplicate, cross-transaction, stale-state, and
  indeterminate evidence instead of advancing controller knowledge.

### State epochs and operation preparation

- Advanced the current canonical state epoch only from a completed effect whose
  exact `libpkgstate` publication receipt or reconciled state proves the supplied
  resulting snapshot.
- Bound every effect request to the exact state snapshot against which its
  application plan was prepared; failed effects from an older epoch are rejected
  as stale as strictly as successful effects.
- Rebased operation preparation on `transaction_progress`, using its current
  state and requiring the selected action to be graph-ready.
- Required install absence and unchanged historical installed authority for
  upgrade or removal in the current epoch, preventing silent reinterpretation of
  a transaction after earlier effects.
- Preserved exact lifecycle-node terminal knowledge after failed operations:
  executed successful nodes remain satisfied, the failed node remains failed,
  and unexecuted members remain blocked.

### Deliberate boundary

- Progression performs no source acquisition, build, check, planning, lifecycle,
  application, publication, restart, or backend effect.
- Added no ready-peer selection policy, parallelism, retry policy,
  transaction-wide rollback, durable progression store, or effectful CLI
  command.
- The existing read-only `catalog`, `resolve`, and `transaction` commands remain
  unchanged.

### Authority floors

The authority floors are unchanged from 0.6.0:

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
- Sealed successful plans into exact `libpkgapply` requests and one-operation
  effect requests, including explicit lifecycle order and install reason
  authority.
- Allowed an effect request to retain the complete transaction program while
  selecting exactly one target action; unrelated package nodes, other target
  actions, and runtime cohorts remain inert until an external scheduler chooses
  them.
- Kept removal independent of construction and incoming artifact projection.

### Deliberate boundary

- Preparation may inspect retained artifact bytes but performs no target
  observation, target lease acquisition, lifecycle execution, application, or
  state publication.
- Added no recursive construction, check execution, cross-package scheduler,
  durable preparation journal, Linux-backend policy, or effectful CLI command.
- Retained construction, durable effect execution, restart, and effect-request
  identity contracts from 0.5.0 and 0.4.0; only effect-request admission was
  widened from the initial single-package program restriction.

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
