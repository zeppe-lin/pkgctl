## Library-level package campaign qualification

Before a user-facing construction command is extended, the non-CLI integration
suite must compose the production controller core through a disposable package
campaign. The campaign must acquire real YAML collection authority, resolve and
compose the dependency graph, prove that a predecessor build becomes ready before
its dependent build, materialize declared local sources through `libpkgfetch`,
and execute both constructions through `libpkgbuild-exec`. The selected package
also carries a real check program; `libpkgcheck` / `libpkgcheck-exec` must admit
that check only after construction and the deterministic process backend must
consume the exact staged source and constructed package trees selected by the
native session locator. Only process execution may be replaced by that fake.

Successful construction must produce a real package archive plus retained
`libpkgbuild-image` authority. The same transaction progression must then become
planner-ready and pass through `libpkgbuild-plan`, `libpkgstate-plan`,
`libpkgplan`, and `libpkgapply` request sealing. That exact request is then
realized by `libpkgapply-posix` under a real POSIX mutation lease against a
disposable target root. The campaign reopens the exact constructed archive,
projects canonical state through `libpkgstate-apply`, durably journals the
controller effect, publishes the new canonical generation, retires the operation
with that resulting snapshot, and verifies both target bytes and installed-state
ownership before requiring terminal transaction progress.

The campaign must also cover mutation over existing state. After a real v1
installation it edits one protected target path locally, reacquires a v2 catalog,
forces catalog preference, and requires a real upgrade. Fresh target facts must
come through `pkgctl::observe_native_target_paths()` from the same
`libpkgapply-posix` observer used by the native frontend; integration tests must
not duplicate application-to-planner fact conversion. The upgrade must retain
local protected bytes, stage the incoming object as rejected evidence, publish
v2 state without claiming the protected path, and retain the exact completed
application evidence.

The same completed evidence is then a qualification seam for the reconciliation
libraries. They are test-only dependencies here, not `pkgctl` production
authority: the campaign projects pending reconciliation, verifies the exact
POSIX rejected-object record, proves duplicate publication is idempotent,
reopens the inventory, resolves the tuple, reopens it again, and proves the
resolved tombstone suppresses resurrection. Finally an exact-convergence
transaction removes the installed package through the normal planner/apply/state
path while the unowned local configuration survives and the resolved
reconciliation record remains durable.

This campaign is deliberately non-CLI and non-privileged: command parsing and
Linux namespace capability are not allowed to be the first place where library
composition defects are discovered. The same transaction must also cross
`native_posix_transaction_run_runtime`, not stop at manually assembled controller
functions. One bounded launch must execute dependency construction, package
construction, check, application, and publication through that composition root.
After the runtime object is destroyed and reopened over the same durable stores,
driving the completed journal must rehydrate the exact retained per-dispatch
operation observations and subordinate effect bodies, return quiescent, preserve
the terminal record, and perform no second archive acquisition or target/state
mutation. Future package/rootfs work should extend this in-process campaign
before adding corresponding frontend behavior.

## Release 0.35.0 bounded native command qualification

The final command suite must prove:

- `--start` and `--resume` are mutually exclusive and require one explicit
  lowercase run nonce, existing roots, inspected interpreter, numeric
  credentials, source-date epoch, and positive step bound;
- start retains the original catalog/state universe before admission and refuses
  an existing journal, while resume requires both the retained universe and
  that exact journal and reproduces the same transaction identity;
- effect bodies are durably retained through owner codecs before journal records
  name them, interrupted application recovery uses direct active-request lookup
  rather than enumeration, and later application/terminal replay does not feed
  a receipt-named historical application journal back into the restart checkpoint;
- runtime namespaces are existing-only and no parse refusal creates runtime,
  build, or target paths; a malformed caller-owned execution root is retained
  as terminal construction evidence and the command surfaces its durable backend
  diagnostic instead of reporting only a generic failed disposition;
- native check execution resets the exact call-scoped temporary host resource
  and prepares `/tmp/home` before entering `libpkgcheck-exec`, while the
  caller-owned execution root view remains untouched;
- target observations are per-dispatch; fresh operation observations are
  retained before effect/run journals can name the admitted session, while
  started or completed replay reloads that exact body by attempt-session and
  performs no fresh target observation; ordinary runtime dependencies use the
  transitive resolver run-scope closure, and lifecycle execution capabilities do
  not become target-mutation identities;
- operation preparation preserves caller-owned normalized path policy through
  real upgrade planning into the sealed application request; a protected
  incoming path can retain observed state, stage the incoming object for
  reconciliation, and relinquish operated-package ownership without the
  controller inventing or rewriting policy;
- one invocation executes no more than `--max-steps`, with no implicit loop,
  sleep, retry, scheduler, repair, rollback, cleanup, compaction, or discovery;
- the existing read-only commands remain byte-stable sensors;
- the actual built `pkgctl` executable can start one synthetic installation,
  stop after one durable construction step, lose access to the live collection,
  resume the exact retained transaction to target/state completion, and report
  the same transaction and journal identities across both processes;
- a second start of the admitted nonce is refused, while a second resume of the
  completed run reports zero durable work and leaves target bytes and canonical
  state unchanged;
- a shared build records a direct `libpkgfetch.so.2` ELF dependency and the
  direct compiler qualification refuses fetch generations outside 2.x; and
- a complete private-prefix shared/static Meson build passes under ASan and
  UBSan before tagging.

## Release 0.34.0 native target/runtime composition qualification

The native runtime suite must prove:

- one composition root owns the POSIX run, construction/check evidence, and
  effect stores plus the native session, operation, archive, progress,
  recovery, driver, and dispatch-nonce chain in dependency-safe lifetime order;
- four path-selected namespaces are existing absolute directories, opened with
  final-component no-follow directory authority, and refused when normalized paths overlap;
  descriptor-selected aliases are refused by device/inode identity;
- construction/check and lifecycle execution retain independent typed
  root-view authority; a deliberately shared path requires one identity, while
  build/check writable roots remain disjoint from lifecycle execution, target,
  and lifecycle-session domains;
- launch requires an explicit durable run-intent nonce and remains bounded by
  one positive drive policy;
- a construction-only transaction executes through the concrete native locator,
  publishes durable evidence, and completes without consulting installed-tree,
  operation-specification, restart-body, archive, application, or lifecycle
  authority;
- reopening that completed journal reconstructs exact progress from retained
  evidence through the same locator and selected backend profile and returns
  quiescent without caller-supplied replacement progress;
- renamed store paths prove descriptor-retained authority remains attached to
  the opened namespaces rather than later pathname lookup; and
- source contracts reject store initialization, directory scanning, target
  observation, backend selection, implicit credentials, unbounded loops,
  retries, workers, cleanup, and command wiring.

The full Meson suite must run under ASan and UBSan against the staged private
prefix before release.

## Release 0.33.0 native operation-authority qualification

The native operation suite must prove:

- one replayable per-dispatch specification must name the exact selected action
  and operation kind and supply explicit lifecycle execution order; foreign,
  incompatible, incomplete, or duplicate lifecycle authority is rejected;
- fresh removal preparation reproduces the canonical effect request through the
  existing preparation path;
- lifecycle-free operation authority accepts an application target with no
  optional lifecycle-executor binding and does not enter `libpkgapply-exec`; a
  non-empty exact lifecycle order still requires the bound executor;
- equivalent durable record, run, and dispatch inputs reproduce the same
  admitted lifecycle session and mechanical effect-attempt nonce;
- locating operation authority creates, removes, scans, stats, opens, executes,
  journals, publishes, or observes no host or target resource;
- planning refusal remains a typed refusal and does not admit an effect attempt;
- restart requires the exact run-retained effect-attempt identity and latest
  effect-journal record before consulting subordinate body authority;
- missing or foreign effect records and contradictory restart bodies fail
  closed through typed errors or the canonical checkpoint validator;
- the explicit archive map opens only an exact incoming authority, passes its
  retained complete-archive digest to the selected backend, returns no archive
  for an unmapped authority, and rejects duplicate or non-absolute coordinates;
- no CLI command, backend selection, retry, scheduler, target profile, or state
  store access enters this boundary.

The full Meson suite must run under ASan and UBSan against the staged private
prefix before release.

## Release 0.32.0 exact progress-rehydration qualification

The progress suite proves that an admitted or ownership-only history performs no
evidence lookup and reproduces the initial progress exactly. Completed
construction evidence is selected by exact journal, dispatch, and attempt,
decoded under caller-owned bodies, and reproduces the same progress after store
reopen. Missing evidence fails with a typed error before context authority is
consulted.

The effect suite proves that an exact terminal checkpoint can reconstruct the
canonical operation result without appending, continuing, observing state, or
publishing. A completed operation history then reproduces the exact progress
and resulting state while leaving the effect journal unchanged. Source
contracts reject execution, resume, append, publication, canonical-state reads,
and filesystem effects from the progress provider.

The state-owner qualification independently tests install, replacement, and
removal projection plus stale and foreign authority refusal. Strict compilation
and release contracts require libpkgstate 3.1.0 or later.

## Release 0.31.0 native locator qualification

The native locator suite must prove:

- the same journal and dispatch reproduce the same construction or check
  session across reserved and started run records;
- path scope depends on the stable run journal, not the changing record head;
- catalog source coordinates are accepted only under the exact retained
  acquisition specification and native direct-package layout;
- catalog-selected inputs reuse exactly one successful predecessor
  construction's package-output resource and path;
- installed-selected inputs consult the caller-owned retained-package source and
  reject a foreign installed package identity or invalid host path;
- check source, package output, and package inputs reuse exact resources retained
  by the successful construction;
- the check source path is obtained from the build adapter's pure prepared-path
  projection;
- locating a session creates, removes, scans, stats, chmods, materializes,
  executes, journals, or advances nothing;
- configuration rejects overlapping writable and root-view domains; and
- no command surface or backend composition is introduced.

The source-contract pass protects the negative boundary. Runtime tests must
exercise both predecessor and installed-package input branches and compare fresh
and restart session identities.

## Release 0.30.0 shared-session recovery qualification

The shared-session suite must prove:

- fresh construction/check execution and restart recovery consult the same
  deterministic session source;
- pure build/check request projection is identical to the request used by
  effectful preparation;
- pure projection creates, removes, stages, chmods, or otherwise touches no
  host resource;
- construction recovery reacquires genuine source material and reproduces the
  retained materialization identity;
- check recovery reproduces the exact admitted execution request;
- backend capability drift and foreign session authority fail before decode;
- operation execution and operation recovery remain delegated to their own
  sources;
- the POSIX runtime recovers a started construction from a reopened evidence
  store without consulting a separate construction recovery provider.

# pkgctl testing

## Qualification roles

The test tree separates four kinds of evidence instead of treating every test
program or shell script as interchangeable:

- `tests/unit/` contains in-process semantic and model tests. These should carry
  combinatorial state-machine and authority edge cases without pretending to be
  operator-boundary qualification.
- `tests/fixtures/` contains deterministic fiction and helper executables used
  only to establish test authority. A fixture is not a production backend and
  must not replace the production path under test.
- `tests/integration/` contains vertical composition tests. Command-boundary tests
  invoke the built `pkgctl` executable and verify observable durable state, target
  effects, restart behavior, output, and exit status. Pre-frontend campaigns may
  instead compose `pkgctl-core` in-process when the purpose is to qualify library
  authority before exposing another command. Such campaigns must keep the real
  owner adapters/stores and may fake only the explicitly isolated actuator.
  Help-text greps are discoverability checks, not substitutes for runtime tests.
- `tests/contracts/` rejects source, dependency, boundary, release, and test-layout
  drift. Contract tests may prove forbidden structure, but do not stand in for
  successful runtime behavior.

`pkgctl:package-pipeline` is the non-privileged pre-frontend package vertical.
It uses real acquisition, resolution, transaction progression, fetch/build/package
authority, check admission, target observation, POSIX application, canonical state
publication, protected upgrade, rejected-object evidence, exact-convergence
removal, and test-only reconciliation persistence. Its process backend is the only
fake actuator and must consume the exact resources admitted by the real adapters.

The same campaign deliberately loses controller durability at two asymmetric
points. Publication interruption occurs after canonical state has already selected
the requested result but before the effect journal records publication terminal;
restart must reconcile by observing that state and perform zero additional
publications. Application interruption occurs after POSIX application has completed
and the exact application receipt body has been retained, but before the effect
journal records application terminal. Restart must load the terminal subordinate
application journal, bind its receipt identity to the retained owner-encoded body,
adopt that already-completed application into the controller journal, reconstruct
the exact historical lease-bound state projection recorded by that journal under
a newly held target lease, perform zero fresh applications and zero application
resumes, and publish exactly once. Upgrade and removal both qualify this rule. A
terminal subordinate journal without its
retained exact receipt body is not enough authority for automatic continuation.

`pkgctl:package-failure-matrix` runs the same harness in failure mode. A definitive
dependency-build failure must fail that node and block its dependent build, check,
and target operation without publishing package state. A definitive package-check
failure must retain successful construction evidence, fail the check, block target
application, and likewise publish no package state. Each case must also be launched
through `native_posix_transaction_run_runtime`, destroy that runtime, and reopen the
failed journal. Rehydration must preserve the stopped failure without another build
or check execution and without consulting operation specifications, archive
authority, effect bodies, target mutation, or canonical publication. These are
transaction progression and durable runtime assertions, not frontend exit-code
tests.

`pkgctl:package-operation-failure-matrix` carries definitive operation failures
through that same native composition root. Lifecycle cases add explicit
pre-install and post-install lifecycle-scoped resolution goals before transaction
composition; recipe program presence alone is not execution authority. Faults are
injected only at the subordinate owner protocol that can produce them: a failed
`libpkgapply` backend
operation, failed pre-install or post-install `libpkgexec` lifecycle actuation,
or a `libpkgstate` publication transaction that fails before publication. The
controller must retain the exact terminal effect outcome and block later work
without manufacturing rollback authority. Application and pre-install lifecycle
failure leave the target unchanged. Post-install lifecycle and publication
failure retain completed application evidence and may therefore leave the target
mutated while canonical state remains at the prior generation. Each case destroys
and reconstructs the native runtime over the same journals; reopen must reproduce
the same stopped progress and terminal effect, load retained subordinate bodies,
and perform no additional construction, check, lifecycle actuation, application,
archive acquisition, rollback, or state publication. The fixture provisions the
configured lifecycle-session parent exactly as the CLI runtime layout does. Every
executed lifecycle session must then appear as one single-use direct child with
its private `tmp/home` beneath that parent; no journal/dispatch intermediate
directory may be required, and reopen must create no additional lifecycle
scratch.

`pkgctl:cli-run` is the privileged native vertical test. Before a fresh run is
admitted, the command checks that the selected Linux backend can establish the
exact execution guarantees implied by the transaction's build, check, and
lifecycle nodes. Capability absence is control-plane unavailability: the command
refuses before retaining the initial command universe or writing a run/evidence
journal. It must not manufacture a package construction/check failure from an
unavailable actuator. The same test supplies construction/check and lifecycle
root views separately and proves that non-supervisor construction/check
credentials are rejected before evidence retention. This preserves the two
execution authority domains without claiming an unsupported credential
transition.

When the test process lacks the required delegated or privileged Linux authority,
`pkgctl:cli-run` prints the missing guarantees and relevant capability probes,
proves that no transaction evidence was retained, then exits 77. Meson records a
skip. A skip is **not** successful vertical qualification. For release
qualification run:

```sh
PKGCTL_REQUIRE_NATIVE_INTEGRATION=1 \
  meson test --suite integration-privileged --print-errorlogs
```

In that mode capability unavailability is a hard failure, so release automation
cannot accidentally count a skipped native actuator as proof. The test must pass
on a context that can establish the complete sealed profile. The generated static
`native-test-interpreter` is both part of the default build graph while tests are
enabled and an explicit Meson dependency of this test. Clean qualification can
therefore run tests without a rebuild step and cannot depend on a fixture left
behind by an earlier build tree.

`pkgctl:cli-run-removal` extends that same privileged boundary through exact
convergence. It first establishes one real installed package, then composes a
build-only goal with `--converge-exact` and requires an explicit removal node.
The run must remove the target object, publish package absence, and retain a
terminal operation effect with completed application evidence whose operation
dispatch names that exact removal node. The mutable source collection is then
removed and a repeated resume must consume retained run authority, perform zero
durable steps, and leave both target and state unchanged. This is
controller/application/state composition evidence; it does not replace the
lower semantic, POSIX, or publication-adapter suites.

`pkgctl:cli-run-application-restart` qualifies a real process death inside the
operation effect. A test-only ptrace supervisor follows only the `pkgctl`
process and kills it immediately after the application-journal directory
successfully synchronizes the first durable active-request index. At that
point the controller effect remains at `application_intent`, the subordinate
`libpkgapply` journal is durable, and no target mutation has started. No
production stop-at-stage option or fault hook is introduced. The supervisor
itself has an ordinary integration test with a two-sync probe: an initial
directory synchronization occurs before any active reference exists, then the
probe atomically publishes and synchronizes a valid active reference. Untraced
the probe must cross that second synchronization; traced it must retain the
reference but die before its post-sync marker.

The restarted command must first refuse when the exact retained operation
observation body is temporarily absent, without advancing either run or effect
journal. Restoring those immutable bytes must let the same nonce resume after
the live collection is removed, consume the exact pre-crash application journal,
complete target mutation and state publication, and retain that journal identity
in terminal effect evidence. Finally, corrupting the active application-journal
locator after terminal completion must not affect a repeated zero-step resume;
subordinate application-journal authority is consulted only while the owning
effect stage is `application_intent`.

`pkgctl:cli-run-publication-restart` qualifies the next recoverable effect
boundary. Its test-only ptrace supervisor recognizes only the canonical POSIX
state selector replacement `renameat(current.tmp.*, current)` on the exact
opened canonical-store directory. The independent ordinary integration probe
contains an unrelated rename before that selector replacement and a marker
after it, then proves both interruption modes: `before-selection` must leave the
old selector plus the unselected temporary, while `after-selection` must expose
the new selector but still kill before the next userspace action.

The privileged vertical runs both physical crash outcomes while the controller
effect is durably at `publication_intent`. In both cases application evidence is
already terminal, target bytes are already installed, transaction evidence and
the exact publication request are retained, and no publication receipt exists.
Removing that exact retained publication-request body must make resume refuse
without advancing the run or effect journal even when the live canonical state
already equals the requested result. After restoring it, the live collection is
absent and the historical active application-journal locator is deliberately
poisoned: restart must not re-enter application. If selection had not happened,
restart republishes the retained request and records a real publication receipt;
if selection had already happened, restart recognizes the resulting snapshot,
seals the effect with reconciled-state evidence, and must not fabricate or repeat
a publication receipt.

Meson exposes `unit`, `integration`, `integration-privileged`, `header`, and
`contract` suites. `tests/run-direct.sh` is the source-tree qualification path
and must link the complete current CLI, including command-only dependencies,
before invoking the integration and contract layers.

## Release 0.29.0 recovery qualification

The evidence-backed recovery suite must prove:

- construction and check evidence are selected only by the exact durable run
  journal, dispatch, and attempt session;
- missing evidence refuses recovery before the context source is called;
- the caller supplies complete semantic bodies rather than result identities;
- every controller session, transaction, graph node, request, materialization,
  execution request, backend profile, subordinate execution, and result identity
  agrees with the durable record;
- the existing `libpkgbuild-exec` and `libpkgcheck-exec` decoders are used and no
  second subordinate codec or controller result parser exists;
- a recovered controller result reproduces the exact fresh canonical identity
  and canonical subordinate encoding after the POSIX store is closed and
  reopened;
- foreign construction sessions, source materializations, execution requests,
  backend profiles, build requests, check requests, or retained constructions
  fail before transaction-run reconciliation;
- operation recovery bypasses the construction/check store and remains delegated
  to the exact effect-journal recovery authority;
- the POSIX transaction runtime owns one store-backed recovery source and borrows
  only the caller's context provider;
- no recovery path calls `from_sha256`, scans for semantic bodies, infers that
  missing evidence means no execution occurred, or fabricates controller result
  identity.

The native Meson pass must run the recovered construction and check tests under
ASan and UBSan against the complete staged dependency closure.

The suite protects authority composition rather than implementation shape.

## Release 0.28.0 durable evidence qualification

The construction/check evidence suite must prove the complete crash barrier:

- started run ownership is durable before the driver is invoked;
- exact canonical subordinate result bytes are admitted only for the same
  journal, transaction, dispatch, node, attempt, request, and execution chain;
- the immutable content object is durable before its typed index;
- terminal run retirement is attempted only after evidence publication;
- evidence-store failure leaves started ownership and no terminal successor;
- final run-commit failure leaves both started ownership and loadable evidence;
- exact publication retries are idempotent, while a conflicting publication for
  one journal/dispatch/attempt fails closed;
- corrupt encodings, corrupt indexes, absent indexed objects, and altered
  content are rejected;
- descriptor anchoring survives path rename and replacement;
- construction and check record domains cannot be substituted for one another;
- the store never reconstructs semantic results from retained identities;
- the POSIX runtime uses four independent caller-opened directory authorities.

The native Meson qualification must run with `tests=enabled` and exercise the
full controller closure under ASan and UBSan. A source-only contract pass is not
a substitute for the runtime store tests.

## Required qualification

Every release must establish:

- strict C++17 compilation under GCC and Clang;
- independent usability of every internal public header;
- explicit request validation and duplicate-goal rejection;
- deterministic authority, request, session, result, and evidence identities;
- path and diagnostic-provenance independence where non-semantic;
- exact existing-state binding and read-only command-store access;
- catalog, resolution, and transaction composition against exact tagged
  dependency sources;
- complete install, upgrade, and removal effect-session paths;
- exact historical versus incoming lifecycle authority during upgrade;
- caller lifecycle order bound to the complete transaction phase set;
- one outer target lease observed before every effect and after publication;
- no state publication after lifecycle or application failure;
- transaction provenance retained in publication requests and durable receipts;
- exact construction-node admission and source/build authority binding;
- real local-source materialization and independent package archive inspection;
- failed builds retained without artifact promotion;
- construction identity independence from host paths;
- exact action/construction/current-state binding before package-local planning;
- immutable transaction progression from exact construction, check, and effect evidence;
- operation-unit readiness with lifecycle phase nodes absorbed without losing
  node-level terminal status;
- simultaneous exposure of independent ready units before dispatch policy;
- deterministic reservation of the first canonical unit allowed by explicit
  construction/check capacities and one hard operation lane;
- explicit reserved, started, completed, and released-unstarted ownership;
- exact predecessor, package-input, state-epoch, and attempt-session binding;
- stop-after-terminal-failure containment without discarding already-started
  work;
- canonical state-epoch advancement only from exact publication or reconciled
  effect authority;
- refusal of out-of-order, duplicate, cross-transaction, stale-state, and
  indeterminate progression evidence;
- retained build/image admission projected without reopening artifact bytes;
- successful install and removal preparation through official adapters;
- typed planner refusal retained without application or effect promotion;
- removal preparation proven independent of incoming artifact authority;
- explicit non-completed and indeterminate publication outcomes;
- CLI usage, authority-failure, and deterministic output contracts;
- malformed YAML reaches the command boundary as the exact typed
  `pkgsource::yaml::yaml_error`, while the controller core remains free of the
  syntax adapter;
- durable intent and terminal snapshots around every irreversible handoff;
- strict codec, causal-shape, committed-head, legacy-chain, and POSIX store
  validation;
- durable run admission before first reservation and exact one-transition
  successor snapshots;
- exact progression rehydration, graph-unit validation, predecessor binding,
  terminal-evidence binding, and active operation state-epoch validation;
- write-ahead restart classification for reserved, construction, check, and
  operation dispatches without outcome inference;
- effect-attempt admission committed before started operation ownership, with
  exact retry after either cross-journal crash window;
- conservative restart at every lifecycle, application, and publication crash
  boundary;
- exact `libpkgapply` journal required for application continuation;
- publication retry only from the exact prior state and reconciliation only
  from the exact resulting state;
- a newly held outer lease required on every resumed attempt;
- pure check-request admission before concrete host resources exist;
- exact check-session resource admission with canonical multi-input projection;
- deterministic check request, session, execution, result, and progression identities;
- refusal of missing, duplicate, forged, aliased, cross-transaction, cross-node,
  stale-construction, duplicate-completion, and driver-contract evidence;
- concurrency-safe check completion after unrelated progression advancement;
- durable release of never-started reservations and exact recovery of started
  construction, check, and operation dispatches;
- refusal of foreign recovery sessions and stale effect-journal checkpoints
  before run storage;
- exact terminal effect-result identity after journal rehydration, independent
  of argument evaluation order;
- no driver or run append when effect restart requires external resolution;
- proof that read-only CLI commands remain sensors, `run` remains the sole
  effect-implying frontend, and exact run/effect inspection performs no mutation;
- release, source, manual, shell, and patch-hygiene contracts.
- caller-configured POSIX transaction runtimes duplicate and retain exact run,
  effect, and target-lock directory authorities while borrowing semantic,
  archive, backend, and canonical-state authorities;
- runtime launch receives one explicit caller run nonce and retains that exact
  nonce in the admitted history;
- canonical dispatch nonce derivation is stable for one exact committed
  record/run pair, changes after a legal successor, and rejects a foreign run;
- runtime drive carries no hidden nonce cache or caller dispatch-nonce service;
- a retained run-store descriptor remains authoritative after the original
  pathname is renamed and replaced, and the replacement directory remains
  untouched;
- bounded runtime launch commits and completes through the native construction
  path, while exact-journal runtime drive reopens only the selected committed
  head;
- construction-only execution does not acquire an archive, create an effect
  journal, or touch the target-lock directory;
- invalid directory descriptors fail through the owning store or native effect
  source before a runtime is returned;
- caller-configured native effect sources duplicate and retain only the selected
  lock-directory descriptor while borrowing explicit backends and stores;
- replayable incoming archives must match both the exact package-image and
  inspection-receipt identities, while removal never consults archive authority;
- fresh execution and continuation recovery derive state through the
  lease-bound canonical-state adapter and share one live POSIX lease across the
  continuation driver and resulting-state observer;
- dropping one shared authority does not release the lease while the other
  remains live, concurrent acquisition fails nonblocking, and full destruction
  permits a fresh acquisition with a new lease identity;
- terminal-success recovery acquires only a state observer, publication recovery
  acquires only publication authority, and neither path opens an archive;
- invalid lock descriptors, absent archives, foreign images, and foreign
  inspection receipts fail before executable target authority is returned;
- continuation, resulting-state observation, and publication reconciliation
  represented as distinct call-scoped authorities;
- fresh execution requires continuation and observation bound to the same live
  lease acquisition before effect admission;
- recovery requests exactly the authority subset implied by the durable effect
  checkpoint and rejects missing or surplus authority;
- terminal success reads canonical state without receiving continuation
  authority, while publication reconciliation receives no obsolete state
  projection;
- one effect driver acquired from the exact per-dispatch execution or recovery
  handoff rather than shared across a transaction run;
- live target-lease and exact expected-state projection validation before fresh
  effect admission;
- driver-source refusal or invalid authority leaves only the durable reservation
  and appends no effect attempt;
- terminal failure and external-resolution recovery request no physical driver,
  while successful terminal recovery may acquire one only to read resulting
  state;

## Exact effect-inspection command tests

The command boundary must prove:

- both the existing store path and exact lowercase attempt identity are required;
- invalid identity syntax is a usage failure before store access;
- one valid command emits the existing deterministic effect-attempt report;
- repeated inspection is deterministic and leaves optional evidence absent;
- the store contents are byte-identical before and after inspection;
- inspection succeeds without a writer-lock file and does not recreate it;
- a missing store is refused without initialization;
- a missing head and a corrupt head retain distinct typed effect-journal
  diagnostics;
- no attempt enumeration, run traversal, semantic rehydration, restart
  construction, driver invocation, append, reconciliation, repair, or mutating
  command path is introduced.

## Durable effect-attempt inspection tests

The effect inspection boundary must prove:

- one exact caller-supplied attempt identity selects one committed head;
- a missing head is refused as a store conflict and a foreign returned attempt
  is refused as a store-contract violation;
- the result retains the storage-derived record and the exact pure restart
  assessment for that record;
- terminal, automatically continuable, and external-resolution-required remain
  distinct predicates, including terminal records that are automatically
  consumable by run reconciliation;
- the report is deterministic and exposes every retained controller-owned
  identity, stage, disposition, outcome, lifecycle count, and completion fact;
- optional predecessor, active-index, application, publication, terminal, and
  reconciled-state fields remain absent until durably retained;
- identities are printed but never rehydrated into lifecycle, application,
  transaction, publication, or state values;
- an empty POSIX store read creates no lock, an existing lock is acquired through
  a read-only shared descriptor, and removing a lock before inspection does not
  recreate it;
- append still establishes the writer lock and retains exclusive writer
  authority;
- inspection performs exactly one store load and no append, driver call, run
  traversal, discovery, reconciliation, repair, scheduling, or command action.

## Exact run-inspection command tests

The command boundary must prove:

- both the existing store path and exact lowercase journal identity are required;
- invalid identity syntax is a usage failure before store access;
- one valid command emits the existing deterministic transaction-run report;
- the store contents are byte-identical before and after inspection;
- inspection succeeds without a writer-lock file and does not recreate it;
- a missing store is refused without initialization;
- a missing head and a corrupt head retain distinct typed diagnostics;
- no journal enumeration, semantic rehydration, effect access, append,
  reservation, execution, reconciliation, repair, or mutating command path is
  introduced.

## Durable transaction-run inspection tests

The inspection boundary must prove:

- a caller-selected journal identity causes exactly one head load and no append;
- a missing head is refused and a foreign returned journal is a store-contract
  violation;
- sequence-zero quiescence is reported as incomplete without semantic
  rehydration;
- reserved ownership is active but requires no external evidence and names the
  `release_reserved` disposition;
- started construction names exact recovery authority and requires external
  evidence;
- terminal failure is classified as stopped and no longer active;
- deterministic reports retain exact durable identities and restart
  dispositions;
- inspection performs no journal discovery, progression rehydration, append,
  reservation, execution, effect-journal access, scheduling, or command action.

## Restart-safe transaction-launch tests

The launch boundary must prove:

- the run nonce is issued for the exact immutable initial run before any store
  access;
- nonce refusal performs no load, append, progression rehydration, dispatch
  nonce issuance, or driver call;
- a missing exact journal head commits sequence zero before bounded driving;
- admission append failure performs no drive action and remains exactly
  retryable;
- a failure after admission leaves the committed head resumable;
- an existing exact sequence-zero or successor head is resumed without another
  admission append;
- retry after completed work returns completed through quiescence without a
  dispatch nonce, run append, or driver invocation;
- a foreign store head is rejected before progression or execution authority is
  requested;
- the returned starting and final records remain in one transaction, nonce,
  dispatch-policy, and journal universe and never move backward;
- launch performs no journal discovery, unbounded execution, worker creation,
  concurrency, adaptive scheduling, retry timing, cleanup, rollback,
  compaction, garbage collection, or command action.

## Durable transaction-run admission tests

The admission boundary must prove:

- the nonce source receives the exact immutable initial run before storage;
- source refusal performs no append and creates no journal authority;
- sequence zero retains no predecessor or dispatch ownership;
- the returned run is reopened from the exact record returned by storage;
- exact retries for the same initial run reissue the same nonce and converge on
  the same record;
- an append failure leaves no claimed admission and remains exactly retryable;
- a store returning foreign admission authority is rejected;
- admission performs no reservation, execution, effect-journal access,
  discovery, scheduling, drive loop, retry timing, rollback, cleanup,
  compaction, garbage collection, or command action.

## Durable transaction-run tests

The run-journal suite must prove:

- all-zero, short, and malformed run nonces are refused;
- identical transaction, policy, nonce, and run values produce identical
  journal and record identities, while another nonce produces another history;
- admission after the first reservation, dispatch ownership at sequence zero,
  positive-sequence histories without a reservation, histories retaining more
  dispatches than committed transitions, and sequence values that disagree with
  retained dispatch states or observations are refused;
- every legal reservation, start, unstarted release, uncertainty observation,
  and terminal completion produces one exact successor;
- no-op successors, skipped states, history rewrites, multiple simultaneous
  record changes, sequence gaps, predecessor forks, and authority changes are
  refused;
- binary encoding is deterministic and round-trips byte-for-byte;
- truncation, trailing bytes, unsupported encoding/schema versions, oversized
  input, invalid enum/boolean/count values, and single-bit corruption are
  refused;
- decoded records recompute policy, unit, dependency, dispatch, record, journal,
  and top-level identities;
- reopening requires the exact transaction, progression, current state, terminal
  flags, graph units, predecessor evidence, completed evidence, and active
  operation state epoch;
- durable successor validation rejects sequence gaps, predecessor forks,
  authority changes, detached reservation progression/state epochs, and
  non-successor transitions before storage;
- POSIX storage rejects writable or non-regular heads and head-selected
  snapshots, missing heads with record entries, corrupt heads, missing selected
  snapshots, malformed uncommitted histories, and foreign same-name records;
- a committed head selects one exact self-contained snapshot without requiring
  historical snapshots to remain present;
- exact admission and successor retries recover interruption before head
  publication and remain idempotent after head publication;
- concurrent exact retries from separate processes converge on one committed
  record under the directory lock;
- append performs lock, immutable-record publication, record and directory
  synchronization, atomic head replacement, and a final directory
  synchronization before reporting success;
- effect snapshots reject sequence values that disagree with retained stage and
  evidence, including lease-loss terminals that conceal unresolved publication
  intent;
- version-one effect histories without a head retain strict full semantic-chain
  validation, while version-two histories require an exact durable head; an
  exact retry of the latest legacy record atomically rewrites that selected
  snapshot as version two before publishing its head;
- reserved work is classified releasable, started construction/check work
  requires external recovery evidence, and started operations require exact
  effect-journal inspection through the retained effect-attempt identity;
- operation-start commitment calls the effect store before the run store,
  verifies both returned authorities, and invokes no driver;
- a committed orphan effect admission leaves the run reservation releasable,
  while a committed started run with no effect snapshot remains unresolved;
- equivalent retries produce identical effect-attempt, dispatch-record, run,
  and run-journal identities, while another effect nonce changes all affected
  ownership identities;
- completed and released runs are quiescent, while uncertainty observations
  remain attached to active operation recovery;
- journaling performs no driver invocation, resource discovery, subordinate
  evidence reconstruction, state publication, cleanup, automatic retry,
  compaction, garbage collection, or CLI action.

## Single-dispatch execution tests

The durable execution suite must prove:

- construction and check drivers are invoked only after the started run
  successor is accepted by the run store;
- effect admission is accepted before operation started ownership, and both are
  accepted before the first lifecycle, application, or publication call;
- a start-store failure invokes no driver and leaves the prior reserved record
  selected;
- an effect-admission failure invokes no operation driver and appends no run
  successor;
- driver escape after durable start leaves the exact started construction,
  check, or operation dispatch restartable;
- a failed terminal run append never fabricates completion and leaves the
  started record selected;
- a terminal effect journal paired with a lost run completion is classified for
  exact effect-journal inspection;
- successful construction, check, and operation execution commits exactly two
  run successors after reservation: start and result;
- successful operation publication advances from a fresh authoritative state
  read, while uncertainty carries no resulting state;
- lost-lease and indeterminate-publication results remain active observations;
- exact store retries are accepted but foreign returned authority is refused;
- the layer performs no reservation, loop, thread creation, backend discovery,
  automatic retry, automatic release, or CLI action.

## Transaction dispatch tests

The dispatch suite must prove:

- zero capacities, zero nonces, malformed hexadecimal nonces, duplicate nonces,
  foreign dispatches, and invalid ledger transitions are refused;
- equivalent progress and policy values produce the same run, reservation,
  record, and dispatch identities;
- canonical ready-unit order is deterministic and capacity exhaustion does not
  mutate the run;
- construction and check capacity is explicit while operation capacity is
  exactly one;
- active dispatches cannot overlap any graph member, including absorbed
  lifecycle nodes;
- unstarted reservations can be released, retain their historical record, and
  cannot reuse their nonce;
- started work cannot be released as unstarted;
- each construction input matches one exact transaction requirement edge and
  the exact selected or retained package authority;
- successful predecessor construction, build-result, artifact, and dispatch
  dependency identities all match before a dependent build starts;
- check-scoped inputs are constructed before the checked package and inherited
  exactly by the later check request;
- a check dispatch accepts only the exact construction already retained by
  progression and captured at reservation;
- an operation dispatch accepts only the exact transaction action, admitted
  effect session, and reserved canonical state epoch;
- duplicate, cross-transaction, cross-node, cross-session, forged-predecessor,
  stale-state, and wrong-kind completion evidence is refused;
- terminal construction, check, and effect evidence delegates progression to
  the existing `advance_*()` authority;
- lost-lease and indeterminate-publication results remain active ordered
  observations and cannot carry resulting state;
- terminal failure prevents new reservations and starting merely reserved work,
  while already-started independent work can still report exact evidence;
- dispatch performs no backend call, resource discovery, retry, thread creation,
  durable persistence, or CLI action.

## Effect authority tests

The effect suite must prove:

- installation executes pre-install, application, post-install, then publication;
- upgrade executes exact historical removal and incoming installation nodes
  around one upgrade application;
- removal executes historical pre/post removal and publishes the empty result;
- installed reason policy survives upgrade and is explicit for installation;
- missing historical control, mismatched action authority, incomplete lifecycle
  sets, and mismatched admitted sessions are refused before effects;
- lifecycle, application, and publication failures retain subordinate evidence
  without being promoted;
- publication is never attempted after pre/post lifecycle failure, incomplete
  application, or pre-publication lease loss;
- lease loss during publication prevents a completed controller outcome even
  when the state backend reports publication;
- driver evidence for another request or authority universe is rejected.


## Construction tests

The construction suite must prove:

- only an exact catalog-backed transaction `build` node is admitted;
- build/check package-input facts match exact resolver selections before acquisition;
- source and content-store coordinates are explicit and path-safe;
- source bytes are admitted through `libpkgfetch`, not trusted by pathname;
- the sealed build request uses the exact observed materialization, resolver
  architectures, input set, and build policy;
- successful artifact evidence includes independent `libpkgimage` inspection;
- backend failure remains a failed build with no published artifact;
- a driver result from another source or build request is rejected;
- equivalent call-scoped host paths do not alter construction identity;
- the same suite passes for root and an ordinary build UID.

## Transaction progression tests

The progression suite must prove:

- progression begins at the transaction resolution's exact installed snapshot;
- exact retain nodes are initially satisfied;
- build and check nodes form individual units while each target action absorbs
  its exact pre/post lifecycle phase nodes;
- internal lifecycle phase edges do not deadlock operation readiness;
- external predecessors across any operation member still gate the whole unit;
- independent ready construction or operation units remain visible together;
- out-of-order or duplicate construction evidence is refused;
- successful construction satisfies only its exact build node;
- failed construction fails that node and blocks graph dependents;
- operation preparation accepts only a ready action and construction already
  retained by the same progression;
- successful effect evidence advances to the exact caller-supplied state proven
  by publication receipt or restart reconciliation;
- definitive failed effects do not advance state and retain exact executed,
  failed, and unexecuted lifecycle-node status;
- effect evidence from another transaction or older state epoch is refused;
- lost-lease and indeterminate-publication effects are not accepted as terminal;
- passed check evidence satisfies only its exact check node;
- failed check evidence preserves the exact failure kind and blocks dependents;
- prepared check evidence survives unrelated progress only while its node and
  retained construction authority remain exact;
- progression itself exposes no execution, publication, scheduler-selection,
  or CLI path.


## Transaction check tests

The check suite must prove:

- only one exact ready check node with one `build_before_check` predecessor is
  admitted;
- the pure controller request binds the same transaction and exact successful
  construction retained by progression;
- concrete source, package, check-input, root-view, temporary, interpreter,
  credential, and limit authority is admitted separately;
- multiple check inputs canonicalize to the sealed request order regardless of
  caller order, with exact `/check/inputs/<identity>` bindings;
- lower execution-admission failures are reported as
  `invalid_check_session`;
- returned check and execution request identities must match the admitted
  session exactly;
- native and custom driver exceptions are contract violations, not terminal
  package-check evidence;
- repeated equivalent requests, sessions, and results have deterministic
  identities;
- duplicate, cross-transaction, foreign-construction, and stale-node evidence
  is refused;
- unrelated ready-unit progress does not invalidate an otherwise exact
  prepared check session.

## Operation preparation tests

The preparation suite must prove:

- only an exact target `install`, `upgrade`, or `remove` node is admitted;
- incoming operations require the matching completed construction node and the
  exact `build_before_target` transaction edge;
- canonical installed truth is projected through `libpkgstate-plan`;
- incoming candidate, artifact, and image facts are projected through
  `libpkgbuild-plan` and admitted through `libpkgapply`;
- projection retains the construction build/image authority, archive digest,
  normalized image identity, entry count, artifact identity, and manifest
  identity without selecting an inspection backend or reading artifact bytes;
- caller target observations, runtime closure, normalized policy, application
  context, execution control, lifecycle order, and installation reason remain
  explicit inputs;
- official planner success seals matching application and effect requests;
- preparation can select one exact action from a transaction that also contains
  other package nodes without treating those nodes as an execution schedule;
- official planner refusal remains terminal and creates no partial plan,
  application request, or effect request;
- removal never invokes incoming-artifact projection;
- no preparation path executes lifecycle, application, publication, or CLI
  effects.

## Bounded serial transaction-drive tests

The construction and effect suites must prove:

- the drive rejects a zero step bound and never exceeds the explicit positive
  bound;
- every iteration delegates to the storage-loading one-step authority rather
  than carrying a caller snapshot between iterations;
- retained reserved or started ownership is reconciled before any fresh nonce
  authority is requested;
- completion and terminal failure stop after the exact producing step;
- operation external-resolution authority stops immediately, invokes no nonce
  source or driver, and appends neither journal;
- incomplete quiescence and explicit step exhaustion remain distinguishable;
- fresh nonce authority is requested only for storage-derived heads that can
  reserve canonical ready work;
- repeated failures before reservation commitment request the same head-derived
  nonce and leave the committed head unchanged;
- a committed release or other successor is a new nonce issuance domain;
- continued outcomes remain in one journal, expose strictly increasing durable
  heads, and cannot follow a stopping outcome;
- the drive creates no worker, concurrency, adaptive scheduling, unbounded
  loop, retry timing, discovery, rollback, cleanup, compaction, garbage
  collection, or CLI action.

## One-step transaction-advancement tests

The construction, check, and effect suites must prove:

- the call loads the committed head selected by the supplied journal identity
  before invoking any semantic authority source;
- a missing or foreign committed head is rejected before progression or
  execution authority is requested;
- exact progression is rehydrated once from the storage-derived record;
- the first active dispatch in durable ledger order is reconciled and no fresh
  reservation or execution-authority request occurs in that call;
- a retained reservation is released, started construction and check work
  accepts only exact recovered attempt evidence, and operation recovery binds
  the exact retained effect attempt;
- external-resolution operation state returns the unchanged run head, invokes
  no driver, and appends neither journal;
- only a quiescent reopened run may reserve the first canonical ready dispatch;
- a missing selected driver or effect store is rejected before reservation is
  appended and before execution authority is requested;
- reservation commitment precedes execution-authority acquisition, durable
  start precedes driver invocation, and terminal commitment follows accepted
  evidence;
- authority-source failure after reservation leaves the exact committed
  reservation, while driver escape leaves the exact committed started record;
- definitive terminal failure makes a later call quiescent rather than selecting
  unrelated new work under the stop-after-failure policy;
- the returned result reopens exactly from its storage-derived record and its
  disposition, dispatch, and evidence shape cannot contradict that record;
- each call reconciles or executes at most one dispatch and performs no loop,
  scheduler, worker, retry, discovery, rollback, cleanup, compaction, garbage
  collection, or CLI action.

## Exact run-authority rehydration tests

The construction, check, and effect suites must prove:

- one caller source invocation returns the complete progression needed to reopen
  the exact durable run;
- foreign progression is rejected by the ordinary durable-record validator;
- malformed record/run or non-reserved fresh context is rejected before the
  execution-authority source is called;
- fresh construction, check, and operation handoffs bind the exact record, run,
  dispatch, admitted session, and operation nonce;
- repeated equivalent authority produces the same handoff identity;
- semantically valid alternate fresh resources remain admissible and produce an
  identity reflecting the concrete admitted authority;
- reserved restart recovery invokes no subordinate evidence source;
- construction and check recovery rejects results from any session other than
  the exact durable started attempt;
- operation recovery rejects any effect checkpoint with a foreign effect session
  or attempt identity;
- the layer performs no journal load or append, reservation, execution,
  reconciliation, discovery, scheduler, loop, retry, or CLI action.

## Durable dispatch-reconciliation tests

The construction, check, and effect suites must prove:

- a restart-classified reserved dispatch is released through one durable
  successor and an exact retry returns the same committed authority;
- recovered construction and check results must bind the exact started attempt
  session before any store append;
- completion uses the ordinary dispatch transition and a lost final append
  leaves the prior started record retryable;
- an operation checkpoint must bind both the run-retained effect attempt and the
  latest exact effect-journal record;
- a terminal effect journal repairs a lost run completion without invoking the
  mutation driver again;
- automatically continuable effect stages resume through the existing effect
  restart path and then commit one run successor;
- external-resolution stages invoke no driver, append no effect record, and
  append no run record;
- reconstructed terminal effect evidence has the same complete semantic fields
  and identity as the original terminal result.

## Durable restart tests

The restart suite must prove:

- a fresh successful attempt records admission, intent, terminal evidence, and
  one terminal controller snapshot in predecessor order;
- interrupted pre- or post-lifecycle intent is never replayed automatically;
- application intent without the exact application journal requires external
  resolution;
- the exact application journal permits `libpkgapply` continuation under the
  newly held lease;
- when the subordinate application journal is already terminal, automatic
  continuation additionally requires the exact retained receipt body and adopts
  that receipt into the missing controller application-terminal fact without
  invoking `libpkgapply` continuation again;
- publication interrupted before state mutation retries the retained request
  only after observing the exact expected prior snapshot;
- publication interrupted after state mutation reconciles the exact resulting
  snapshot without a second publication;
- mismatched subordinate evidence, stale controller records, contradictory
  installed state, or a lost replacement lease is rejected;
- immutable POSIX snapshots remain readable, non-writable, non-replacing, and
  discoverable across repeated directory scans.

## Negative authority tests

Tests must also reject:

- empty collection or resolution requests;
- relative canonical-store paths;
- duplicate semantic goals;
- missing target-binding options;
- missing or mismatched canonical stores;
- transaction-only policy on another command;
- unknown goal scopes, lifecycle actions, and collection revisions;
- effect requests whose selected action, application authority, or lifecycle
  phase set does not match the retained transaction program;
- reintroduction of provisional pkgctl package-operation or legacy pkgman
  semantics.

## Linkage

Shared qualification must inspect the executable's direct dependency closure.
Static qualification must use static authority archives where available. An
unavailable external static dependency is reported rather than silently
replaced by a shared object while claiming a fully static result.

## Replay

Published mboxes are independently applied to the supplied base bundle. Final
trees and stable patch-ID columns must match, and each replayed commit boundary
must pass its applicable tests.

## Publication-terminal process restart

The privileged `pkgctl:cli-run-publication-terminal-restart` vertical kills the
controller only after the state selector has been replaced and the subsequent
`publication_terminal` effect-journal head has been durably synchronized. The
restart must consume the exact retained publication request and receipt, seal
the effect terminal without application or publication replay, and remain a
zero-step operation on repeated resume. Removing the retained publication
receipt before restart must fail without advancing either durable journal.

The non-privileged `pkgctl:publication-terminal-interrupt-fixture` probe
qualifies the ptrace boundary independently: an unrelated state selection is
followed by an effect-head replacement and effect-directory synchronization;
the supervisor must kill after that synchronization and before the next
userspace marker.

## Lifecycle-intent process restart

The privileged `pkgctl:cli-run-lifecycle-resolution` vertical qualifies the
existing conservative restart policy at both lifecycle intent stages without
adding a subordinate lifecycle journal or automatic replay rule.

The CLI fixture requests lifecycle authority explicitly in addition to its
normal run goal: `lifecycle:pre-install=fixture` for the pre-install case and
`lifecycle:post-install=fixture` for the post-install case. This is required by
the transaction boundary: `libpkgtransaction` creates lifecycle nodes only from
explicit lifecycle-scoped resolution goals and does not infer executable work
merely because a selected recipe declares a lifecycle program. Start and resume
therefore carry the same typed lifecycle goal, while resume still recomposes from
the retained command universe rather than rediscovering live collection bytes.

The pre-install case interrupts immediately after the durable
`before_lifecycle_intent` effect head is synchronized and before lifecycle
execution is entered. The target and canonical state must remain untouched.
Resume must report `external-resolution-required`, append zero durable steps,
create no lifecycle execution session, and preserve the exact run/effect heads.

The post-install case uses a package with only post-install lifecycle. It
interrupts immediately after the durable `after_lifecycle_intent` head is
synchronized. At that point application evidence and target bytes are already
present while canonical state publication has not begun. Resume must again
report `external-resolution-required`, append zero durable steps, leave target
bytes and canonical state unchanged, and create no lifecycle execution session.

Both cases remove the live collection before resume and repeat the resume. The
controller therefore demonstrates that this stop is derived from retained
historical authority and remains stable; it neither re-resolves the package nor
speculatively executes the ambiguous external action.

The non-privileged `pkgctl:lifecycle-intent-interrupt-fixture` probe qualifies
the interruption mechanism independently. The supervisor observes only successful
`.tmp-effect-head-* -> <64-hex>.pjeh` replacements followed by successful
`fsync()` of the exact watched effect-store directory. It counts distinct durable
record identities per effect head, so an intentional idempotent republication of
the same admitted record does not advance the interruption boundary. It kills
after the second distinct durable record for `before-lifecycle-intent` and after
the fourth for `after-lifecycle-intent`. The probe explicitly republishes its
first record before advancing, proving that duplicate durable head publication is
ignored. The CLI test separately proves the semantic stage represented by the
selected head.
