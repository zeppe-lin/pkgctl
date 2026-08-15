# pkgctl history

## Unreleased

- Renders structured native construction and check execution failures in the CLI,
  including execution classification and termination status even when the invoked
  program writes no diagnostic output.
- Uses the common `PKG_SOURCE_ROOT` / `PKG_PACKAGE_ROOT` check recipe
  environment from libpkgcheck-exec 0.6.0 and rejects resurrection of the retired
  branded check-variable dialect.
- Adds privileged archive-source build/check qualification that distinguishes
  retained source objects from the archive-realized construction workspace and
  verifies the independent check view of the sealed package image.
- Reconstructs native check source and package resources from retained source
  materialization and sealed artifact/image authority instead of borrowing
  construction-session staging or package-output residue. Candidate check inputs
  are reconstructed from their own predecessor artifacts as well. The
  archive-source qualification now deletes those construction-private trees
  before check resume and requires successful independent resource realization.
  Concrete `pkgexec::resource_identity` values are minted by pkgctl per semantic
  check resource instance rather than borrowed from source/image realizers, so
  distinct package inputs remain distinct even when their image bytes are equal.
- Phase-separates concrete package inputs: construction receives only
  build-scoped resources even though the sealed build request retains both
  build/check requirements. Candidate check inputs are realized only for the
  check phase and installed check inputs are located from state-owned retained
  resources, so one package selected in both scopes cannot alias a construction
  resource.
- Aligns transaction scheduling with that phase boundary: build-scoped
  requirements gate construction, check-scoped requirements gate the independent
  check node, and the checked package's own construction remains a separate
  check predecessor. Qualification now rejects the obsolete model where a
  check-only dependency delays construction or appears as a construction
  resource.

## 0.38.0 - 2026-08-15

- Makes durably started construction and check dispatches restartable after
  process death by retaining the exact admitted attempt session before the
  `started` run record becomes durable. Recovery reopens that immutable attempt
  authority when terminal result evidence is absent; it does not reconsult the
  live collection, session locator, current configuration, or installed-package
  lookup.
- Orders construction artifact publication after durable terminal construction
  evidence. Native build execution first seals and independently verifies the
  archive beneath the private attempt session; `pkgctl` then retains the exact
  construction result before projecting those retained bytes into the caller's
  public artifact root. Evidence-backed restart completes or verifies that
  projection without rerunning construction or treating filesystem residue as
  historical truth.
- Qualifies live construction/check execution requirements from retained runtime
  evidence below the command frontend. A started attempt with no terminal result
  requires the matching native backend for exact replay, while terminal evidence
  can retire the run without reacquiring execution authority. Transaction-run
  evidence storage remains outside the CLI/frontend boundary.
- Adds deterministic privileged process-death qualification at durable
  construction `started`, durable check `started`, and construction artifact
  publication. The ordinary unprivileged pass preflights native isolation before
  ptrace and skips these cases when the required Linux namespace guarantees are
  unavailable.
- Renames the historical release ledger to `HISTORY.md`. Tagged release entries
  are immutable release facts; unreleased implementation work is not backfilled
  into the latest tag.

## 0.37.0 - 2026-08-13

- Closes the native source-4 ABI generation across source codec/YAML, catalog
  owner/codec/acquisition, resolver, fetch, build execution, transaction, check,
  source-plan, build-image, state-source-backed application, and their execution
  adapters. The 0.37 runtime no longer admits a source-3 retaining carrier beside
  `libpkgsource.so.4`; retained source snapshots therefore cross one coherent
  binary generation instead of relying on mixed C++ layouts. Historical
  dependency ledgers remain unchanged. The closure is version-addressable:
  source begins at 4.1.0, source-yaml at 2.0.0, fetch and build-exec at 3.0.0,
  exec at 2.1.1, the Linux backend at 0.6.2, application core/execution at
  3.0.1, and the POSIX application provider at 3.2.1, so a clean resolver cannot
  select their source-3-linked predecessor releases.
- Adds `pkgctl build PACKAGE`, a constrained construction/check frontend over
  the same sealed transaction and durable native run kernel as `pkgctl run`.
  The command owns the exact package build goal, optionally adds its check goal,
  and prefers catalog construction authority. After composition the frontend
  requires a catalog-backed build node for that exact package (and a check node
  when requested), so installed authority cannot silently satisfy the public
  build verb without construction. It accepts no lifecycle,
  target-mutation, state-publication, convergence, or caller-supplied goal policy.
- Makes the caller-selected existing `--artifact-root` explicit public build
  authority. It is disjoint from the private runtime hierarchy, participates in
  native construction-session root qualification, and is retained with the
  frontend kind in immutable command evidence. Resume refuses a different
  artifact root or another frontend before durable advancement, while terminal
  replay reports the same retained artifact inventory without reacquiring live
  catalog semantics.
- Reports successful construction artifacts deterministically with package
  release identity, exact retained archive path, sealed artifact identity and
  digest, byte count, build-result identity, and binding/image identities. A
  privileged build campaign crosses a bounded start/resume boundary through the
  real POSIX-shell native construction fixture, verifies dependency and selected
  package archives beneath the public artifact root, executes the selected check,
  leaves canonical target state unchanged, and proves target-operation/private
  artifact namespaces remain absent.

## 0.36.0 - 2026-08-13

- Separates construction/check-only native run composition from target-operation
  authority. A sealed transaction with no install, upgrade, remove, or lifecycle
  nodes uses only the run, evidence, and effect namespaces plus construction/check
  session and archive mechanisms. It carries no operation specification/restart
  source, target-lock namespace, application backend, lifecycle backend, canonical
  state publication backend, operation session store, or operation effect body
  authority. Supplying any of those authorities to the construction-only native
  composition is refused rather than silently retained.
- Makes the shared dispatch and recovery composers represent absent operation
  authority as absence. Construction/check calls remain valid; an impossible
  operation execution or recovery request fails closed instead of relying on a
  fabricated placeholder authority.
- Routes `pkgctl run` through that reduced composition whenever the sealed
  transaction contains no target operation. The command does not open the
  lifecycle root, target root, target-lock store, application stores, effect-body
  store, or canonical-state publication backend for such a run. A privileged
  construction-only CLI campaign proves one package archive is produced while
  those namespaces remain absent, canonical target state remains unchanged, and
  terminal resume needs no reconstructed collection semantics.
- Adds a privileged native-construction campaign that executes actual
  POSIX-shell recipe bodies through `libpkgexec-linux` rather than the synthetic
  static interpreter fixture. The caller-owned build root receives only the exact
  shell runtime plus one explicit `chmod` runtime needed to seal an executable
  fixture payload; no general host tool tree is exposed. A fetched local-source
  dependency is built first, publishes source-derived data and an executable tool,
  and the selected package both reads that predecessor package tree and executes
  the tool directly from its read-only build-input resource. Its real check consumes
  the staged source and constructed package tree, and both immutable package archives
  are inspected for the bytes produced by those programs. Canonical target state
  and all target-operation namespaces remain untouched.

## 0.35.1 - 2026-08-12

- Refuses corrupted transaction-run journal, effect-journal, and retained
  run-evidence regular-file authority without blocking. Read-only lock, head,
  index, snapshot, and evidence-body opens are nonblocking before regular-file
  validation, so replacing retained private authority with a FIFO fails closed
  instead of wedging restart or inspection. Alarm-guarded owner tests pin the
  refusal path.

## 0.35.0 - 2026-08-12

- Corrects progress-scoped execution qualification so live interpreter and supervisor
  authority are required only by a resume that still owns unexecuted process work.
  The lifecycle-resolution campaign now proves wrong interpreter and lifecycle credentials
  are refused before advancement, while the simple install campaign proves operation-only
  resume ignores an unrelated interpreter coordinate after construction has completed.
- Keeps that lifecycle authority campaign capability-gated when the full test set is
  run without privileged native execution. Its live-work setup now treats native preflight
  unavailability exactly like the other privileged verticals: skip in development runs,
  fail loudly when `PKGCTL_REQUIRE_NATIVE_INTEGRATION=1`.

### Bounded native transaction command

- Adds `pkgctl run`, the first and only effect-implying frontend in this
  release line. Before fresh-run retention or admission, the command proves
  that the selected native Linux backend can establish the execution guarantees
  implied by the transaction; actuator unavailability is refused as control
  state rather than recorded as package failure. `--start` admits one explicit
  run nonce; `--resume` requires that exact retained command evidence and exact admitted
  journal while refusing a second semantic transaction request. Both perform at
  most the positive `--max-steps` bound through the reviewed native runtime.
- Corrects the privileged progress-scoped resume fixture so supervisor credentials
  are dropped after the dynamic loader maps the build-tree CLI closure. Reduced-credential
  qualification now measures pkgctl execute-now authority rather than access to shared
  libraries under the developer build root.
- Preserves sanitizer loader ordering in that credential-context fixture: when the
  launcher itself is using dynamic AddressSanitizer, its already-loaded runtime remains
  first while the credential hook and inherited preload chain are added without displacing
  it. Sanitizer startup therefore remains loader authority rather than the uid/gid test.
- Uses one current fail-closed private command-evidence proof format. Before run
  admission it retains the complete start-only
  transaction inputs, the exact admitted interpreter identity, the exact admitted
  construction/check/lifecycle backend capability profiles through the canonical
  libpkgexec owner encoding, and the original owner-encoded catalog and canonical-state
  snapshots. Each durable construction/check attempt also retains the exact owner-encoded
  backend profile required by its historical result decoder. Durable lifecycle results
  retain the exact libpkgexec profile body inside the libpkgapply-exec owner encoding, so
  completed lifecycle evidence likewise needs no command-level profile surrogate. Recovery
  does not query a current execution backend or inject a command-level recovery profile. Resume supplies
  only the current canonical-store pathname plus live runtime/actuator authority; retained
  state supplies target binding and retained transaction semantics recompose the same
  transaction identity. Historical execution evidence is validated against retained
  profiles, while current interpreter/backend/credential preflight is required only
  for scopes that can still execute. Completed or externally blocked runs therefore
  do not re-prove unused actuator authority or re-observe the interpreter pathname.
  Re-declaring start-only semantics is a usage error. Bytes outside the current
  private proof format fail closed; there is no decoder or migration path.
- Retains lifecycle results, application receipts, publication requests, and
  publication receipts in a private immutable command store before an effect
  journal may name them. Bodies use their owning codecs and are validated again
  by the existing restart checkpoint. Construction dispatch evidence retains
  the exact controller-owned admitted construction session plus the canonical
  libpkgfetch materialization body; check dispatch evidence retains the exact
  controller-owned admitted check session. Restart decodes those sessions under
  retained transaction/progress/node/source authority instead of reconsulting
  current construction/check configuration, installed-package resource lookup,
  or the old source universe. Transaction-run evidence uses one first-generation
  private schema and first-generation record identity domains; incompatible development bytes fail closed
  with no compatibility decoder.
- Uses `libpkgapply-posix` 3.1.0 direct active-request lookup only to recover
  an unresolved application-intent journal, without scanning a journal
  directory or moving application-storage policy into the controller. Later
  application/terminal history is validated from retained owner evidence. The
  application journal itself retains the exact admitted state-projection body;
  current canonical state is observed separately and never used to reconstruct
  that historical projection.
- Requires explicit existing runtime, build, and target roots; an inspected
  interpreter; numeric credentials; a hermetic source-date epoch; and any
  retained installed-package trees. Every runtime namespace must already
  exist. The command initializes, enumerates, discovers, repairs, cleans, or
  garbage-collects none of them.
- Observes the target only for a fresh current operation dispatch and immutably
  retains the exact observation set before effect/run journals can name the
  admitted session. Started and completed replay reload that body by the
  run-retained attempt-session identity instead of re-observing a target the
  operation may already have mutated. Future reserved operations still receive
  fresh observations. Ordinary runtime dependencies come from the exact
  transitive resolver run-scope closure, and application mutation identity stays
  separate from lifecycle execution capability identity.
- Adds no daemon, scheduler, timer, implicit retry, unbounded run loop,
  automatic rollback, journal discovery, ambient configuration, or hidden
  replanning. Step-limit completion is successful and explicitly resumable;
  failed, externally blocked, and quiescent-incomplete states remain failures.
- Keeps construction/check execution and lifecycle execution as separate command
  authority domains with independent existing root views and explicit credential
  sets. The current Linux backend admits only supervisor credentials; incompatible
  explicit credentials are refused before transaction admission rather than
  reclassified as package failure. No fakeroot or ownership virtualization is
  implied.
- Keeps the read-only CLI smoke test synchronized with the split `run --start` /
  `run --resume` usage grammar and makes help-surface mismatches diagnostic rather
  than silent `set -e` exits.
- Hardens shell qualification against option-like expected text: every variable
  fixed-string `grep` pattern is separated from grep options, and the test-layout
  contract rejects unsafe forms. Diagnostics beginning with `--` are therefore
  tested as data rather than accidentally parsed as grep options.
- Organizes qualification by semantic role and adds a privileged process-level
  `pkgctl run` start/resume test. Capability-unavailable hosts may skip that
  vertical scenario in ordinary development runs, but release qualification
  requires `PKGCTL_REQUIRE_NATIVE_INTEGRATION=1` and treats such a skip as a
  hard failure. The generated static native interpreter is part of the default
  test build graph and remains an explicit test dependency, so clean or
  no-rebuild qualification cannot accidentally rely on a stale fixture.

### Pre-frontend package campaign qualification

- Moves live application-to-planner target observation conversion out of the CLI
  and into `pkgctl-core`, preserving the existing
  `pkgctl/native-target-observations/1` identity domain while leaving retention
  and replay policy with the runtime/frontend.
- Extends the non-CLI package campaign through a locally modified protected-path
  upgrade, real rejected-object publication, v2 canonical-state replacement,
  exact-convergence package removal, and durable reconciliation
  publication/resolution/anti-resurrection checks. The reconciliation libraries
  are test-only qualification dependencies and do not change the production
  dependency contract below.
- Adds deterministic durability/failure qualification to the same package
  campaign. Publication interruption after canonical selection must reconcile
  without publishing twice; application interruption after POSIX completion must
  reopen the exact terminal application journal, adopt its durably retained
  receipt into the controller journal without either a fresh apply or an
  application resume, reconstruct the journal-bound historical state
  projection under fresh target exclusion, and continue to publication. A
  separate mode proves definitive dependency-build and package-check failures block
  dependent target work and publish no package state; those same failures cross
  the native runtime, survive destroy/reopen from durable evidence, and neither
  rerun the failed execution actuator nor acquire operation/archive authority.
  A second runtime matrix carries definitive application, pre/post-lifecycle,
  and canonical-publication failures through their owning subordinate protocols.
  Destroy/reopen must retain the exact terminal effect without retry or rollback:
  application/pre-lifecycle failure leaves the target unchanged, while
  post-lifecycle/publication failure may leave completed application files on the
  target with canonical state still at the previous generation.
- Adds a native-runtime uncertainty matrix that distinguishes durable publication
  intent from terminal indeterminate publication evidence. Intent plus an already
  visible exact result reconciles without republication; intent plus the exact
  prior generation permits one execution of the retained publication request.
  A terminal indeterminate receipt with that prior state remains
  `external-resolution-required` and is never discarded to manufacture an
  automatic retry. The bounded drive now reports that block on the same durable
  step that retains the non-retiring observation instead of misreporting a
  coincident step-limit outcome. A lifecycle intent with no terminal process evidence likewise
  remains externally blocked across destroy/reopen, commits zero further durable
  steps, and performs no physical replay or archive reacquisition.
- Corrects the outer-lease runtime qualification to respect the operation
  advancement evidence contract: fresh execution returns the exact write-ahead
  effect admission while the terminal lease-loss record is loaded separately
  from the effect journal. Fresh driving now expects one specification call and
  no replay; explicit reopen performs the retained semantic reconstruction.
- Adds a native-runtime target-lease contention boundary. The concrete POSIX
  source translates only nonblocking `lock_busy` into a generic
  `mutation-authority-unavailable` controller disposition. Fresh contention
  releases its unstarted reservation before returning and creates no effect
  attempt; recovery contention leaves the started run/effect heads unchanged.
  Bounded driving stops immediately in either case, performs no waiting or
  implicit retry, and a later explicit drive may continue after the competing
  holder releases. The package authority fixture now retains target observations
  per admitted operation session so released reservations and later fresh
  attempts do not share invented replay authority.
- Qualifies fresh target-lock contention through the privileged `pkgctl run`
  frontend with the real POSIX lease provider. The test derives and holds the
  exact command mutation domain before `--start`, requires
  `mutation-authority-unavailable` with a released-unstarted operation and no
  effect attempt, proves duplicate `--start` still refuses the already-admitted
  run, removes the live collection, then releases the holder and completes via
  one explicit `--resume` using a new operation dispatch. No new CLI option,
  waiting policy, or implicit retry is introduced.
- Qualifies the complementary already-started contention window through the
  privileged frontend. A real application-intent interruption leaves one operation
  dispatch/effect attempt durable; a competing real POSIX lease then forces
  `--resume` to return `mutation-authority-unavailable` with zero durable
  advancement and identical run/effect heads. After holder release, a later explicit
  `--resume` must complete that same dispatch and effect attempt, not allocate a
  replacement, even after the live collection is removed.
- Qualifies real outer-lease loss through the privileged `pkgctl run` frontend.
  A post-install lifecycle fixture blocks on a FIFO handshake while an external
  revoker unlinks the command's single anchored POSIX target lock, guaranteeing
  loss after application/lifecycle completion and before publication. `--start`
  must report `external-resolution-required` with one started operation and one
  terminal `outer-lease-lost` effect. Effect inspection keeps its own restart
  classification (`terminal`, automatically continuable, not externally blocked);
  the run/effect join owns the non-retiring external-resolution control state.
  Target effects remain while canonical state
  stays prior. Later `--resume` calls, including after live collection removal,
  preserve the same run/effect heads with zero durable advancement and never
  recreate the missing target lock.
- Qualifies POSIX outer-lease loss through the native runtime and fixes the
  run/effect restart join exposed by that test. A terminal `outer_lease_lost`
  effect is a non-retiring transaction observation: restart commits it once if
  the run journal missed the observation, and the bounded drive stops for
  `external-resolution-required` on that same durable step. An already-retained
  identical observation yields the same block instead of duplicate submission. The matrix revokes the real anchored lock after post-install
  lifecycle work and during successful publication, proving that target/state
  truth is preserved without replay or accidental completion.
- Drives the same package transaction through the production
  `native_posix_transaction_run_runtime` composition root, including real
  application and canonical publication. The integration test then destroys and
  reopens the runtime over the same journals and requires completed-operation
  rehydration to use caller-retained target observations and subordinate effect
  bodies, remain quiescent, and perform no second archive acquisition or target
  mutation.
- Keeps lifecycle-executor authority genuinely optional for lifecycle-free
  operations. Native operation admission does not invoke `libpkgapply-exec` when
  the transaction's exact before/after lifecycle order is empty; non-empty
  lifecycle orders retain the existing target-bound executor validation.
- Binds every executable lifecycle scratch session as one deterministic direct
  child of the caller-provisioned lifecycle-session root. The child identity is
  domain-separated by run journal, dispatch, phase, and index, so
  `libpkgapply-exec` can materialize its single-use leaf without requiring
  unowned intermediate directories or filesystem mutation in operation authority.

### Release qualification

- Raises the declared Meson floor to 1.6.0, matching the integration test graph's
  use of executable targets as `test()` arguments instead of advertising an older
  configure-time contract that Meson itself warns is unsupported.
- Restores canonical ATX document hierarchy and pins `meson.options`, documentation
  structure, and complete contract-test registration as release-source invariants.

### Dependency contract

Current 0.35.0 source accepts exactly these direct dependency generations:

- libpkgsource >= 3.0.0, < 4.0.0
- libpkgsource-yaml >= 1.0.0, < 2.0.0 (CLI only)
- libpkgsource-plan >= 1.0.0, < 2.0.0
- libpkgcatalog >= 3.0.0, < 4.0.0
- libpkgcatalog-codec >= 3.0.0, < 4.0.0 (CLI only)
- libpkgcatalog-acquire >= 3.0.0, < 4.0.0
- libpkgstate >= 3.1.0, < 4.0.0
- libpkgstate-posix >= 3.0.0, < 4.0.0
- libpkgstate-plan >= 3.0.0, < 4.0.0
- libpkgstate-apply >= 3.1.0, < 4.0.0
- libpkgfetch >= 2.1.0, < 3.0.0
- libpkgbuild >= 3.0.0, < 4.0.0
- libpkgbuild-exec >= 2.2.0, < 3.0.0
- libpkgbuild-image >= 1.0.0, < 2.0.0
- libpkgbuild-plan >= 1.0.0, < 2.0.0
- libpkgimage >= 0.4.0, < 1.0.0
- libpkgplan >= 0.3.0, < 1.0.0
- libpkgexec >= 2.1.0, < 3.0.0
- libpkgexec-linux >= 0.6.0, < 1.0.0 (CLI only)
- libpkgapply >= 3.0.0, < 4.0.0
- libpkgapply-posix >= 3.1.0, < 4.0.0
- libpkgapply-exec >= 3.0.0, < 4.0.0
- libpkgresolve >= 3.0.0, < 4.0.0
- libpkgtransaction >= 3.0.0, < 4.0.0
- libpkgcheck >= 0.2.0, < 1.0.0
- libpkgcheck-exec >= 0.4.0, < 1.0.0

## 0.34.0 - 2026-08-07

### Native target and runtime composition

- Adds `native_posix_transaction_run_runtime`, one stable native POSIX
  composition root for an exact sealed transaction. It owns the selected run, construction/check evidence, and
  effect stores together with the native session locator, operation authority,
  archive map, exact progress rehydrator, restart chain, dispatch nonce
  projection, and construction/check/effect drivers.
- Accepts four explicit existing POSIX namespaces either as absolute paths or
  directory descriptors. Path opening is existing-only, final-component no-follow, and
  directory-typed; all four namespaces must be disjoint by normalized path and
  by retained device/inode authority. The runtime creates or discovers no
  namespace.
- Keeps construction/check root-view authority distinct from lifecycle
  execution-root authority while validating shared-path identity consistently.
  Build/check writable roots may overlap neither lifecycle execution nor target
  or lifecycle-session authority.
- Reuses one semantic progress input for fresh, restart, and completed-history
  projection. Construction/check session realization now depends on exact
  transaction progress rather than caller run intent, and completed operation
  replay passes through the same native operation authority and canonical
  checkpoint validation.
- Keeps live per-dispatch operation specifications, installed-package resource
  retention, subordinate effect-restart bodies, and all selected physical
  backends with their owning callers. The composition root wires these
  authorities; it does not freeze target observations, discover credentials,
  choose backends, or become a new target profile.
- Requires an explicit durable run-intent nonce for every launch and retains the
  existing positive bounded-drive contract. It exposes no mutating command,
  scheduler, retry policy, worker, journal discovery, store initialization,
  cleanup, compaction, or garbage collection.

### Dependency contract

- No dependency floor changes.

## 0.33.0 - 2026-08-07

### Native operation execution and recovery authority

- Adds one replayable per-dispatch operation-specification source plus one fixed
  transaction/lifecycle configuration. The source supplies exact target context,
  execution control, target observations, package policy, runtime closure and
  installation reason where the current operation kind requires them; the fixed
  configuration supplies lifecycle root views, paths, and credentials.
- Prepares each fresh operation through the existing state, build-plan,
  planner, application, and effect boundaries. Install and upgrade consume the
  exact successful predecessor construction already retained by transaction
  progress; removal consumes no incoming artifact authority.
- Requires the replayable per-dispatch specification to supply explicit
  lifecycle execution order. The existing effect boundary validates that the
  order contains exactly the lifecycle nodes attached to the action by sealed
  transaction phase edges; deterministic graph storage is never promoted into
  execution precedence. Lifecycle programs come only from `libpkgapply-exec`,
  and call-scoped sessions are admitted beneath stable run-journal and dispatch
  identities. Repeated acquisition against one durable head reproduces the same
  operation session and mechanical attempt nonce.
- Implements operation restart from the same reconstructed session, the exact
  run-retained effect-attempt identity, the latest effect-journal record, and
  caller-owned subordinate restart bodies. The existing
  `effect_restart_checkpoint::make()` validator remains authoritative for every
  lifecycle, application, publication, and application-journal body.
- Adds an explicit replayable archive source mapping one admitted incoming
  package authority to one absolute retained archive path and one
  caller-selected `libpkgimage` backend. Opening asserts the exact archive
  digest already retained by incoming authority; no directory scan or filename
  inference exists.
- Performs no target observation, mutation, execution, effect-journal append,
  canonical-state read, backend selection, credential discovery, retry,
  scheduling, or command actuation. Stable target/runtime composition and the
  first narrow mutating command remain later bounded releases.

### Dependency contract

- No dependency floor changes.

## 0.32.0 - 2026-08-07

### Exact transaction-progress rehydration

- Adds one production `transaction_progress_rehydration_source` that begins from
  the sealed transaction and replays only completed durable dispatches when
  their exact graph unit becomes ready. Reserved, started, released, and
  indeterminate ownership never becomes semantic progress.
- Selects construction and check evidence by exact run journal, dispatch, and
  attempt identities, then decodes it through the existing owner codecs under
  caller-supplied complete semantic bodies. Durable identities are never
  promoted into requests, materializations, backend profiles, or results.
- Rehydrates completed operations only from an exact terminal effect record and
  a validated checkpoint. The pure terminal path performs no continuation,
  journal append, target observation, publication, or retry.
- Advances successful operation state through the state owner's pure
  `project_publication_request()` projection rather than duplicating package
  delta semantics or reopening canonical storage.
- Requires the reconstructed progress identity, current state epoch, completion,
  and failure flags to reproduce the durable run record exactly. Missing,
  foreign, contradictory, or graph-unresolvable evidence fails closed.
- Adds no operation-execution authority, replayable archive provider, backend or
  credential composition, run-intent policy, scheduler, retry loop, repair, or
  mutating command. The next closure is native operation/effect-recovery
  authority over the existing effect boundary.

### Dependency contract

- libpkgstate >= 3.1.0, < 4.0.0
- libpkgbuild-exec >= 2.2.0, < 3.0.0
- libpkgcheck-exec >= 0.4.0, < 1.0.0

## 0.31.0 - 2026-08-07

### Native construction/check session locator

- Adds one controller-private deterministic session/resource locator for native
  construction and check dispatches. The same journal and dispatch authority
  yields the same admitted session before execution and after restart.
- Corroborates each catalog source coordinate against the exact acquisition
  specification and native one-package-directory layout; a diagnostic source
  origin alone is never promoted into filesystem authority.
- Resolves catalog-selected build inputs only from successful predecessor
  constructions retained in exact transaction progress. Installed-selected
  inputs are borrowed through a caller-owned retained-package tree source and
  must name the exact installed package authority.
- Allocates construction sessions, package outputs, artifacts, and check
  temporary roots beneath explicit configured domains using only the stable run
  journal and dispatch identities. Location performs no filesystem observation
  or mutation, source materialization, backend construction, execution, journal
  I/O, or progress advancement.
- Reuses the build adapter's pure prepared-path projection for the staged source
  tree consumed by checks and reuses the exact subordinate resource identities
  already retained by successful construction evidence.
- Makes check admission consume the pure canonical execution-request projection
  directly; no effectful preparation is invoked to establish request authority.
- Adds no progress rehydration provider, operation-execution provider, archive
  source, final target/runtime profile, scheduler, retry policy, or mutating
  command. The next controller closure is exact transaction-progress
  rehydration over the existing durable authorities.

### Dependency contract

- libpkgbuild-exec >= 2.2.0, < 3.0.0
- libpkgcheck-exec >= 0.4.0, < 1.0.0

## 0.30.0 - 2026-08-07

### Shared construction/check session authority

- Replaces separate fresh-execution and restart-context inputs in the POSIX
  runtime with one deterministic construction/check session source. The same
  durable record, run, and dispatch must reproduce the same admitted session in
  both paths.
- Adds operation-only execution and recovery sources and composes them with the
  shared session source, preserving effect-journal ownership of operation
  restart.
- Adds pure canonical execution-request projections to `libpkgbuild-exec` and
  `libpkgcheck-exec`. Recovery can reproduce exact request authority without
  invoking effectful workspace/source preparation.
- Adds native construction/check recovery context composition: construction
  reacquires genuine `libpkgfetch` materialization, both paths reproduce the
  canonical execution request, and both use capability evidence from the
  already selected backend before the durable decoder runs.
- Qualifies exact runtime construction recovery through the shared session
  source and exact check recovery through the same native context composition.
- Adds no concrete session/resource locator, package-input materializer, process
  adoption, scheduler, retry policy, mutating command, or durable encoding.

### Dependency contract

- libpkgbuild-exec >= 2.1.0, < 3.0.0
- libpkgcheck-exec >= 0.4.0, < 1.0.0

## 0.29.0 - 2026-08-07

### Evidence-backed construction and check recovery

- Adds a typed recovery-context source for the exact semantic bodies which the
  durable construction/check store deliberately does not own: the admitted
  controller session, subordinate execution request, backend capability
  profile, and, for construction, genuine source-materialization authority.
- Adds a store-backed recovery-authority source which selects evidence by the
  exact run journal, dispatch, and attempt session; validates every retained
  transaction, node, request, materialization, execution, backend, and result
  identity; invokes the existing canonical build/check decoder; and rebuilds
  the controller result only when its canonical identity is reproduced.
- Makes the caller-configured POSIX transaction runtime own that store-backed
  recovery composition. Callers provide context bodies, not reconstructed
  construction/check results and not identity-shaped substitutes. Operation
  recovery remains delegated to the effect-journal authority boundary.
- Missing evidence remains unresolved started work. It is never translated
  into "the dispatch did not run," released ownership, or a fabricated result.
  Foreign context, contradictory decoded evidence, and decoder refusal fail
  through typed durable-evidence errors before run reconciliation.
- Qualifies exact construction and check replay after closing and reopening the
  POSIX evidence store, rejection of absent and foreign context, operation
  delegation, and the runtime-owned recovery topology.
- Adds no context discovery, source/resource reconstruction, process adoption,
  scheduler, retry policy, garbage collection, mutating command, or new
  subordinate durable encoding.

### Dependency contract

- No dependency floor changes.

## 0.28.0 - 2026-08-06

### Durable construction and check evidence barrier

- Adds typed construction-dispatch and check-dispatch evidence records bound to
  the exact run journal, transaction, dispatch, graph node, attempt session,
  controller result, subordinate request/backend/execution identities, and the
  existing canonical `libpkgbuild-exec` or `libpkgcheck-exec` result encoding.
- Publishes each exact encoding as an immutable content object and then
  publishes one immutable typed index selected by journal, dispatch, and
  attempt identity. The descriptor-anchored evidence store makes the object
  durable before the index; both are synchronized, collision-safe, and fail
  closed on corruption or conflicting publication.
- Commits the started transaction-run successor before execution, publishes
  construction/check evidence after the driver returns, and only then commits
  terminal run retirement. A failed evidence publication or final run commit
  leaves the exact started ownership durable for recovery.
- Extends the caller-configured POSIX transaction runtime with a fourth
  caller-opened directory authority for construction/check evidence.
- Treats the evidence store as physical durable memory, not semantic model
  authority: no semantic result is reconstructed from identities. Complete
  restart still requires caller-owned providers for source materialization,
  request, execution-request, backend-profile, and concrete resource bodies.
- Adds no evidence discovery, semantic deserialization service, scheduler,
  retry policy, garbage collector, mutating command, or new subordinate codec.
- Enables the project test suite by default.

### Dependency contract

- No dependency floor changes.

## 0.27.0 - 2026-08-05

### Resolver-backed construction and planner-ready application authority

- Seals each construction request directly from resolver-issued authority in
  the exact transaction resolution and selected build node; callers can no longer reconstruct build
  inputs from release, source, artifact, or result identities.
- Replaces unissued package-input tree identities with explicit call-scoped
  resources bound to exact logical `libpkgbuild` input identities.
- Projects check inputs from the admitted build request and admits their
  concrete resources only at the `libpkgcheck-exec` boundary.
- Consumes retained `libpkgbuild-image` authority during operation preparation;
  archive bytes are not reopened or re-inspected by the controller.
- Uses the standalone pure `libpkgbuild-plan` projection and the POSIX state
  generation store under their corrected authority boundaries.
- Collapses the undeployed effect-attempt encoding and controller identity
  domains to their first actual generation. The one effect-journal format
  requires a checksummed durable head; no imaginary legacy history is decoded.
- Adds no package-input materializer, scheduler, discovery policy, retry loop,
  or mutating command.
- Keeps `libpkgsource-yaml` at the command diagnostic boundary: the controller
  core does not link the syntax adapter, while the CLI reports its exact typed
  parser failures without translating them into controller authority.

### Dependency contract

- libpkgsource >= 3.0.0, < 4.0.0
- libpkgsource-yaml >= 1.0.0, < 2.0.0
- libpkgsource-plan >= 1.0.0, < 2.0.0
- libpkgcatalog >= 3.0.0, < 4.0.0
- libpkgcatalog-acquire >= 3.0.0, < 4.0.0
- libpkgstate >= 3.0.0, < 4.0.0
- libpkgstate-posix >= 3.0.0, < 4.0.0
- libpkgstate-plan >= 3.0.0, < 4.0.0
- libpkgstate-apply >= 3.0.0, < 4.0.0
- libpkgfetch >= 1.0.0, < 2.0.0
- libpkgbuild >= 3.0.0, < 4.0.0
- libpkgbuild-exec >= 2.0.0, < 3.0.0
- libpkgbuild-image >= 1.0.0, < 2.0.0
- libpkgbuild-plan >= 1.0.0, < 2.0.0
- libpkgimage >= 0.4.0, < 1.0.0
- libpkgplan >= 0.3.0, < 1.0.0
- libpkgexec >= 1.4.0, < 2.0.0
- libpkgapply >= 3.0.0, < 4.0.0
- libpkgapply-posix >= 3.0.0, < 4.0.0
- libpkgapply-exec >= 2.0.0, < 3.0.0
- libpkgresolve >= 2.0.0, < 3.0.0
- libpkgtransaction >= 2.1.0, < 3.0.0
- libpkgcheck >= 0.2.0, < 1.0.0
- libpkgcheck-exec >= 0.3.0, < 1.0.0

## 0.26.0 - 2026-08-01

### Explicit run intent and canonical dispatch nonce authority

- Adds `canonical_transaction_dispatch_nonce()` and
  `canonical_transaction_dispatch_nonce_source`.
- Derives each fresh dispatch nonce from the exact committed journal, record,
  and reopened run identities through domain-separated SHA-256 identity
  construction.
- Returns the same nonce for an exact retry against one committed head and a
  different nonce after every legal successor head.
- Validates that the supplied run is the exact semantic reopening of the
  committed record before deriving authority.
- Removes run and dispatch nonce sources from
  `transaction_run_runtime_authorities`.
- Requires `posix_transaction_run_runtime::launch()` to receive one explicit
  `transaction_run_nonce`, because that nonce distinguishes caller intent
  between otherwise identical durable histories.
- Makes the POSIX runtime own the stateless canonical dispatch-nonce source, so
  restart requires no hidden nonce cache or nonce side store.
- Performs no random generation, seed management, run-intent persistence,
  journal discovery, semantic rehydration, scheduling, retry, or command action.
- Adds no mutating command.

### Dependency contract

- No dependency floor changes.

## 0.25.0 - 2026-08-01

### Caller-configured POSIX transaction-run runtime

- Adds `posix_transaction_run_runtime`, a caller-configured composition root for
  the existing durable transaction-run controller.
- Retains caller-opened transaction-run, effect-attempt, and target-lock
  directory authorities through the existing descriptor-anchored POSIX stores
  and effect source.
- Owns one POSIX run store, one POSIX effect store, native construction and check
  drivers, and one POSIX per-dispatch effect-driver source.
- Borrows replay-safe run and dispatch nonce sources, semantic progression and
  execution/recovery sources, archive authority, physical backends, and the
  canonical state store; those policy-bearing objects remain caller-owned.
- Adds `launch()` for one exact caller-supplied progression and dispatch policy
  under a positive drive bound.
- Adds `drive()` for one exact caller-supplied journal identity under a positive
  drive bound, with no journal enumeration or latest-run selection.
- Preserves the existing durable admission, reservation, per-dispatch authority,
  execution, effect, publication, and recovery barriers without adding another
  state machine.
- Proves descriptor anchoring by completing a native construction run after the
  original run-store pathname is renamed and replaced; the replacement, effect
  store, and target-lock store remain untouched.
- Performs no path or journal discovery, store initialization, nonce policy,
  semantic evidence construction, backend or archive selection, credential
  selection, waiting, retry loop, scheduling, cleanup, repair, or compaction.
- Adds no mutating command.

### Dependency contract

- libpkgsource >= 2.0.0
- libpkgsource-yaml >= 2.0.0
- libpkgsource-plan >= 2.0.0
- libpkgcatalog >= 2.0.0
- libpkgcatalog-acquire >= 2.0.0
- libpkgstate >= 2.3.0
- libpkgstate-plan >= 2.3.0
- libpkgstate-apply >= 2.4.0
- libpkgfetch >= 1.0.0
- libpkgbuild >= 2.0.0
- libpkgbuild-exec >= 1.0.0
- libpkgbuild-plan >= 2.0.0
- libpkgimage >= 0.3.0
- libpkgplan >= 0.2.0
- libpkgexec >= 1.3.0
- libpkgapply >= 2.2.0
- libpkgapply-posix >= 2.2.0
- libpkgapply-exec >= 1.0.0
- libpkgresolve >= 2.0.0
- libpkgtransaction >= 2.1.0
- libpkgcheck >= 0.1.0
- libpkgcheck-exec >= 0.1.1

## 0.24.0 - 2026-08-01

### Caller-configured POSIX per-dispatch effect runtime

- Adds `posix_transaction_effect_driver_source`, the first concrete native
  implementation of the per-dispatch effect-authority source.
- Requires the caller to select and retain the application backend, lifecycle
  execution backend, canonical state store, replayable-archive source, and one
  already-open target lock directory.
- Duplicates and validates the caller's lock-directory descriptor once, then
  acquires a fresh nonblocking POSIX outer target mutation lease for each exact
  execution or recovery handoff.
- Derives continuation state through the lease-bound `libpkgstate-apply 2.4.0`
  adapter, so current canonical state and projection evidence cannot be supplied
  independently or fabricated by the caller.
- Adds `acquire_transaction_effect_archive()` to open and validate one
  replayable archive against the incoming authority's exact package-image and
  inspection-receipt identities; removal requests do not consult the source.
- Returns continuation and resulting-state observation sharing one lease
  acquisition for fresh execution and continuation recovery.
- Returns only target-scoped state observation for terminal success and only
  publication reconciliation authority for retained publication recovery.
- Keeps the lease alive until every returned authority sharing that acquisition
  is destroyed and releases it automatically on acquisition failure.
- Performs no path discovery, backend construction, credential selection,
  archive selection, waiting, retry, scheduling, journal I/O, cleanup, or
  frontend mutation policy.
- Adds no mutating command.

### Dependency contract

- libpkgsource >= 2.0.0
- libpkgsource-yaml >= 2.0.0
- libpkgsource-plan >= 2.0.0
- libpkgcatalog >= 2.0.0
- libpkgcatalog-acquire >= 2.0.0
- libpkgstate >= 2.3.0
- libpkgstate-plan >= 2.3.0
- libpkgstate-apply >= 2.4.0
- libpkgfetch >= 1.0.0
- libpkgbuild >= 2.0.0
- libpkgbuild-exec >= 1.0.0
- libpkgbuild-plan >= 2.0.0
- libpkgimage >= 0.3.0
- libpkgplan >= 0.2.0
- libpkgexec >= 1.3.0
- libpkgapply >= 2.2.0
- libpkgapply-posix >= 2.2.0
- libpkgapply-exec >= 1.0.0
- libpkgresolve >= 2.0.0
- libpkgtransaction >= 2.1.0
- libpkgcheck >= 0.1.0
- libpkgcheck-exec >= 0.1.1

## 0.23.0 - 2026-08-01

### Split effect continuation and state authority

- Separates physical effect continuation from canonical-state observation and
  publication reconciliation.
- Adds `transaction_effect_state_observer` for target-scoped canonical reads and
  `transaction_effect_publication_driver` for exact retained publication retry.
- Replaces one returned per-dispatch driver with exact execution and recovery
  authority bundles.
- Requires fresh execution continuation and resulting-state observation to name
  the same live lease acquisition before subordinate effect admission.
- Classifies recovery before acquisition and rejects missing or surplus
  continuation, observation, or publication authority.
- Allows successful terminal recovery to read resulting state without lifecycle
  or application authority, and publication reconciliation to proceed without
  fabricating the obsolete pre-application state projection.
- Keeps terminal failure and external-resolution recovery free of physical
  target authority.
- Adds no concrete native source, lease acquisition, archive or backend
  discovery, scheduler, retry policy, cleanup, or mutating command.

### Dependency contract

- libpkgsource >= 2.0.0
- libpkgsource-yaml >= 2.0.0
- libpkgsource-plan >= 2.0.0
- libpkgcatalog >= 2.0.0
- libpkgcatalog-acquire >= 2.0.0
- libpkgstate >= 2.3.0
- libpkgstate-plan >= 2.3.0
- libpkgstate-apply >= 2.3.0
- libpkgfetch >= 1.0.0
- libpkgbuild >= 2.0.0
- libpkgbuild-exec >= 1.0.0
- libpkgbuild-plan >= 2.0.0
- libpkgimage >= 0.3.0
- libpkgplan >= 0.2.0
- libpkgexec >= 1.3.0
- libpkgapply >= 2.2.0
- libpkgapply-exec >= 1.0.0
- libpkgresolve >= 2.0.0
- libpkgtransaction >= 2.1.0
- libpkgcheck >= 0.1.0
- libpkgcheck-exec >= 0.1.1

## 0.22.0 - 2026-08-01

### Per-dispatch effect-driver authority

- Replaces the run-wide operation driver with a caller-owned
  `transaction_effect_driver_source` that returns one call-scoped driver for an
  exact validated execution or recovery handoff.
- Acquires fresh operation authority only after the reservation successor is
  committed and before the subordinate effect attempt is admitted.
- Validates the returned live target mutation lease and proves its state
  projection names the operation session's exact expected snapshot and
  ownership inventory.
- Leaves a source refusal or invalid physical authority as a durable reserved
  dispatch and appends no effect-attempt record.
- Classifies recovery before acquiring physical authority. Terminal failure and
  external-resolution states request no driver; successful terminal recovery
  may acquire one only to read the resulting state required by run progression.
- Keeps drivers call-scoped: they are not serialized, retained in journals,
  shared across operation dispatches, or reconstructed from durable identities.
- Adds no concrete lease, state-store, archive, backend, credential, or path
  discovery; no scheduler, worker, concurrency, retry policy, cleanup, native
  runtime assembly, or mutating command.

### Dependency contract

- libpkgsource >= 2.0.0
- libpkgsource-yaml >= 2.0.0
- libpkgsource-plan >= 2.0.0
- libpkgcatalog >= 2.0.0
- libpkgcatalog-acquire >= 2.0.0
- libpkgstate >= 2.3.0
- libpkgstate-plan >= 2.3.0
- libpkgstate-apply >= 2.3.0
- libpkgfetch >= 1.0.0
- libpkgbuild >= 2.0.0
- libpkgbuild-exec >= 1.0.0
- libpkgbuild-plan >= 2.0.0
- libpkgimage >= 0.3.0
- libpkgplan >= 0.2.0
- libpkgexec >= 1.3.0
- libpkgapply >= 2.0.0
- libpkgapply-exec >= 1.0.0
- libpkgresolve >= 2.0.0
- libpkgtransaction >= 2.1.0
- libpkgcheck >= 0.1.0
- libpkgcheck-exec >= 0.1.1

## 0.21.0 - 2026-08-01

### Exact effect-attempt inspection command

- Adds `pkgctl inspect-effect --effect-store PATH --attempt SHA256` to expose
  the existing durable effect-attempt inspection sensor on the read-only command
  surface.
- Requires one explicitly named existing POSIX store and one exact lowercase
  SHA-256 attempt identity; the command performs no store or attempt discovery.
- Delegates durable classification to `inspect_effect_attempt()` and output to
  the existing deterministic `render_report()` implementation.
- Preserves typed usage, missing-store, missing-head, corruption, conflict, and
  storage-contract diagnostics under the `effect journal` diagnostic class.
- Leaves committed effect stores byte-for-byte unchanged, including when the
  writer lock is absent, and never recreates that lock during inspection.
- Keeps optional controller evidence absent until durably retained and never
  promotes lifecycle, application, transaction, publication, or state
  identities into semantic objects.
- Adds no attempt enumeration, latest-attempt selection, run-journal traversal,
  semantic recovery, restart checkpoint construction, driver invocation,
  append, reconciliation, repair, scheduling, cleanup, compaction, garbage
  collection, or mutation.

### Dependency contract

- libpkgsource >= 2.0.0
- libpkgsource-yaml >= 2.0.0
- libpkgsource-plan >= 2.0.0
- libpkgcatalog >= 2.0.0
- libpkgcatalog-acquire >= 2.0.0
- libpkgstate >= 2.3.0
- libpkgstate-plan >= 2.3.0
- libpkgstate-apply >= 2.3.0
- libpkgfetch >= 1.0.0
- libpkgbuild >= 2.0.0
- libpkgbuild-exec >= 1.0.0
- libpkgbuild-plan >= 2.0.0
- libpkgimage >= 0.3.0
- libpkgplan >= 0.2.0
- libpkgexec >= 1.3.0
- libpkgapply >= 2.0.0
- libpkgapply-exec >= 1.0.0
- libpkgresolve >= 2.0.0
- libpkgtransaction >= 2.1.0
- libpkgcheck >= 0.1.0
- libpkgcheck-exec >= 0.1.1

## 0.20.0 - 2026-08-01

### Durable effect-attempt inspection

- Adds `inspect_effect_attempt()` to load and classify one exact
  caller-selected durable effect-attempt head without semantic rehydration.
- Returns the storage-derived `effect_attempt_record` together with the existing
  pure `effect_restart_assessment` used by executable restart.
- Rejects missing heads and storage responses belonging to another attempt with
  typed store-conflict and store-contract diagnostics.
- Exposes terminal, automatically-continuable, and
  external-resolution-required as separate inspection predicates.
- Adds deterministic effect-attempt reports containing controller-owned attempt,
  record, session, nonce, predecessor, stage, lifecycle-result, application,
  transaction, publication, terminal, and reconciled-state identities.
- Keeps optional evidence absent until durably retained and never promotes an
  identity into lifecycle, application, transaction, publication, or state
  semantics.
- Changes the POSIX effect-store load path to acquire an existing lock through a
  shared read-only descriptor instead of creating or opening the writer lock.
- Leaves empty and committed effect stores unchanged when inspected, including
  when the writer lock is absent, and rechecks under a lock established by a
  concurrent writer.
- Leaves append as the sole `O_RDWR | O_CREAT` and `LOCK_EX` path.
- Adds no CLI, attempt enumeration, latest-attempt discovery, run-journal
  traversal, restart checkpoint construction, driver invocation, append,
  reconciliation, repair, scheduling, cleanup, compaction, garbage collection,
  or mutation.

### Dependency contract

- libpkgsource >= 2.0.0
- libpkgsource-yaml >= 2.0.0
- libpkgsource-plan >= 2.0.0
- libpkgcatalog >= 2.0.0
- libpkgcatalog-acquire >= 2.0.0
- libpkgstate >= 2.3.0
- libpkgstate-plan >= 2.3.0
- libpkgstate-apply >= 2.3.0
- libpkgfetch >= 1.0.0
- libpkgbuild >= 2.0.0
- libpkgbuild-exec >= 1.0.0
- libpkgbuild-plan >= 2.0.0
- libpkgimage >= 0.3.0
- libpkgplan >= 0.2.0
- libpkgexec >= 1.3.0
- libpkgapply >= 2.0.0
- libpkgapply-exec >= 1.0.0
- libpkgresolve >= 2.0.0
- libpkgtransaction >= 2.1.0
- libpkgcheck >= 0.1.0
- libpkgcheck-exec >= 0.1.1

## 0.19.0 - 2026-07-31

### Exact transaction-run inspection command

- Adds `pkgctl inspect-run --run-store PATH --journal SHA256` to expose the
  existing durable transaction-run inspection sensor on the read-only command
  surface.
- Requires one explicitly named existing POSIX store and one exact lowercase
  SHA-256 journal identity; the command performs no store or journal discovery.
- Delegates durable classification to `inspect_transaction_run()` and output to
  the existing deterministic `render_report()` implementation.
- Preserves typed usage, missing-store, missing-head, corruption, conflict, and
  storage-contract diagnostics without translating them into mutation policy.
- Changes the POSIX run-store load path to acquire an existing lock through a
  shared read-only descriptor instead of creating or opening the writer lock.
- Leaves an empty caller-created store directory and an inspected committed store
  byte-for-byte unchanged, including when the writer lock is absent.
- Rechecks under the shared lock when a first writer establishes the lock during
  an initially unlocked read.
- Proves valid, invalid-identity, missing-store, missing-head, corrupt-store, and
  immutable-inspection command behavior against the real POSIX store.
- Adds no semantic progression rehydration, effect-journal inspection, append,
  reservation, execution, reconciliation, repair, scheduling, cleanup,
  compaction, garbage collection, or mutating command.

### Dependency contract

- libpkgsource >= 2.0.0
- libpkgsource-yaml >= 2.0.0
- libpkgsource-plan >= 2.0.0
- libpkgcatalog >= 2.0.0
- libpkgcatalog-acquire >= 2.0.0
- libpkgstate >= 2.3.0
- libpkgstate-plan >= 2.3.0
- libpkgstate-apply >= 2.3.0
- libpkgfetch >= 1.0.0
- libpkgbuild >= 2.0.0
- libpkgbuild-exec >= 1.0.0
- libpkgbuild-plan >= 2.0.0
- libpkgimage >= 0.3.0
- libpkgplan >= 0.2.0
- libpkgexec >= 1.3.0
- libpkgapply >= 2.0.0
- libpkgapply-exec >= 1.0.0
- libpkgresolve >= 2.0.0
- libpkgtransaction >= 2.1.0
- libpkgcheck >= 0.1.0
- libpkgcheck-exec >= 0.1.1

## 0.18.0 - 2026-07-31

### Durable transaction-run inspection

- Adds `inspect_transaction_run()` to load and classify one exact caller-selected
  durable run head without semantic progression rehydration.
- Rejects missing heads and storage responses belonging to another journal.
- Adds `assess_transaction_run_record()` as the shared pure ownership
  classification used by both read-only inspection and rehydrated restart
  checkpoints.
- Classifies completed, stopped-after-failure, active, and quiescent-incomplete
  durable states while retaining exact active-dispatch restart dispositions.
- Adds deterministic transaction-run reports containing controller-owned journal,
  record, run, progression, state, policy, dispatch, attempt, observation, and
  restart evidence.
- Proves initial, reserved, started, stopped, missing-head, and foreign-head
  behavior without any store append.
- Adds no semantic-evidence reconstruction, journal discovery, effect-journal
  inspection, reservation, execution, scheduler, worker, retry policy, cleanup,
  compaction, garbage collection, or command action.

### Dependency contract

- libpkgsource >= 2.0.0
- libpkgsource-yaml >= 2.0.0
- libpkgsource-plan >= 2.0.0
- libpkgcatalog >= 2.0.0
- libpkgcatalog-acquire >= 2.0.0
- libpkgstate >= 2.3.0
- libpkgstate-plan >= 2.3.0
- libpkgstate-apply >= 2.3.0
- libpkgfetch >= 1.0.0
- libpkgbuild >= 2.0.0
- libpkgbuild-exec >= 1.0.0
- libpkgbuild-plan >= 2.0.0
- libpkgimage >= 0.3.0
- libpkgplan >= 0.2.0
- libpkgexec >= 1.3.0
- libpkgapply >= 2.0.0
- libpkgapply-exec >= 1.0.0
- libpkgresolve >= 2.0.0
- libpkgtransaction >= 2.1.0
- libpkgcheck >= 0.1.0
- libpkgcheck-exec >= 0.1.1

## 0.17.0 - 2026-07-31

### Restart-safe transaction launch

- Adds `launch_transaction_run()` to compose exact run admission with the
  existing explicitly bounded serial drive.
- Derives one expected journal identity from the immutable initial run,
  dispatch policy, and caller-owned replay-safe run nonce before storage.
- Loads only that exact journal and appends sequence zero only when no committed
  head exists.
- Requires an existing head to retain the exact transaction, run nonce, and
  dispatch-policy admission universe before it may be resumed.
- Requires an existing sequence-zero head to match the complete expected
  admission record.
- Makes launch retries converge after either admission or later successor
  commits instead of attempting to republish sequence zero over advanced
  history.
- Proves that nonce refusal performs no store access, admission append failure
  performs no drive action, and a post-admission failure leaves the journal
  resumable.
- Proves that retry after completed work performs no admission append, dispatch
  nonce issuance, run append, or driver invocation.
- Returns the storage-derived starting record together with the complete bounded
  drive result and validates that the call remains in one journal, transaction,
  nonce, and dispatch-policy universe without moving backward.
- Clarifies that raw `admit_transaction_run()` retry convergence applies while
  sequence zero remains the committed head; restart-safe launch owns convergence
  after successor records exist.
- Adds no journal discovery, unbounded execution, worker, concurrency, adaptive
  scheduling, retry timing, backoff, resource or evidence discovery, process
  adoption, rollback, cleanup, compaction, garbage collection, or
  effect-implying CLI command.
- Retains the complete authority floors published for 0.16.0.

## 0.16.0 - 2026-07-31

### Durable transaction-run admission

- Adds a caller-owned replay-safe `transaction_run_nonce_source` keyed to one
  exact immutable initial transaction run.
- Requires exact retries for the same initial run identity to return the same
  run-history nonce.
- Adds `admit_transaction_run()` to construct the initial run, derive sequence
  zero, append it durably, validate the complete authority returned by storage,
  and reopen the run from that committed record.
- Makes the sequence-zero store append the sole admission commit point; local
  run and record construction grants no durable authority.
- Requires every admitted record to retain no predecessor and no dispatch
  ownership.
- Proves nonce-source refusal before storage, append failure without claimed
  authority, exact retry convergence, and rejection of foreign store returns.
- Adds no journal advancement, reservation, execution or recovery authority,
  driver invocation, effect-journal access, scheduler, drive loop, retry timing,
  discovery, rollback, cleanup, compaction, garbage collection, or
  effect-implying CLI command.
- Retains the complete authority floors published for 0.15.0.

## 0.15.0 - 2026-07-31

### Bounded serial transaction drive

- Adds a caller-owned replay-safe `transaction_dispatch_nonce_source` whose
  issuance domain is the exact storage-derived run record and rehydrated run.
- Requests a fresh dispatch nonce only when the committed head has no retained
  ownership, is not stopped, and exposes canonical ready work.
- Requires exact retries against an unchanged head to return the same nonce;
  committed successor heads may issue distinct attempt authority.
- Preserves the legacy direct-nonce one-step contract while preventing the
  serial drive from speculatively consuming nonces for recovery or quiescence.
- Adds `transaction_run_drive_policy` with an explicit positive maximum step
  count and no implicit or unbounded default.
- Adds `drive_transaction_run()` as serial composition of the existing
  storage-loading one-step boundary; every iteration reloads committed state.
- Gives retained reservation or started ownership absolute precedence over
  every fresh nonce request and reservation.
- Stops distinctly on completion, terminal failure containment, operation
  external-resolution authority, incomplete quiescence, or step-limit
  exhaustion.
- Returns the exact ordered one-step outcomes and validates that continued
  outcomes remain in one journal, follow strictly increasing durable heads,
  and never follow a stopping outcome.
- Proves exact retry when failure occurs after nonce issuance but before
  reservation commitment: the store head remains unchanged and the same
  head-derived nonce is requested again.
- Adds no worker, concurrency, adaptive priority, unbounded loop, retry timing,
  backoff, resource or evidence discovery, process adoption, rollback, cleanup
  policy, compaction, garbage collection, or effect-implying CLI command.
- Retains the complete authority floors published for 0.14.0.

## 0.14.0 - 2026-07-31

### One-step transaction advancement

- Adds one bounded controller call that loads the committed run head selected by
  an exact journal identity and rehydrates its semantic progression.
- Gives the first active dispatch in durable ledger order precedence over every
  fresh reservation and returns after reconciling that one ownership record.
- Releases never-started reservations, accepts exact recovered construction or
  check evidence, and inspects the exact retained operation effect attempt.
- Returns unchanged durable run authority when operation recovery requires
  external resolution, with no driver invocation and no journal append.
- Permits fresh reservation only when the reopened run has no active ownership,
  using the existing first-canonical-ready dispatch policy.
- Validates the selected driver and required effect store before committing the
  reservation, so missing execution dependencies create no ownership.
- Commits the reservation before requesting fresh execution authority and then
  delegates start, driver, and terminal barriers to the existing durable
  single-dispatch execution functions.
- Leaves exact reserved ownership when authority acquisition fails and exact
  started ownership when a driver or terminal append fails.
- Returns a storage-derived, self-validating advancement result that binds the
  durable record, reopened run, disposition, retained dispatch, and semantic
  evidence accepted or produced by the step.
- Executes or reconciles at most one dispatch and never performs both actions in
  one call.
- Adds no loop, scheduler, worker, concurrency, retry or backoff policy,
  resource or evidence discovery, process adoption, rollback, cleanup policy,
  compaction, garbage collection, or effect-implying CLI command.
- Retains the complete authority floors published for 0.13.0.

## 0.13.0 - 2026-07-31

### Exact run-authority rehydration

- Adds an injected source for the exact semantic `transaction_progress` required
  to reopen one durable transaction-run record.
- Adds caller-owned fresh execution-authority sources for construction, check,
  and operation dispatches, including an explicit caller-issued effect nonce.
- Validates every fresh authority against the exact reserved dispatch through
  the existing pure start transition, without storing or executing it.
- Adds caller-owned recovery-authority sources for exact construction results,
  check results, and effect restart checkpoints.
- Requires recovered construction and check evidence to retain the exact started
  attempt session and operation evidence to retain both the exact effect session
  and effect-attempt identity.
- Obtains no subordinate evidence for a reservation classified as never started.
- Seals deterministic call-scoped handoff identities over the durable record,
  reopened run or restart assessment, selected dispatch, and supplied authority.
- Distinguishes valid fresh resource variation from restart evidence, which is
  never substitutable for the exact durable attempt.
- Rejects malformed durable context before invoking a caller authority source and
  rejects foreign returned authority before any store or driver can be touched.
- Adds no journal discovery or access, reservation, execution, reconciliation,
  evidence serialization, scheduler, loop, retry policy, rollback, compaction,
  garbage collection, or effect-implying CLI command.
- Retains the complete authority floors established by Version 0.10.0.

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
