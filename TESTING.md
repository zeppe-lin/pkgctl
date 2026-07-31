# pkgctl testing

The suite protects authority composition rather than implementation shape.

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
- artifact reinspection reproducing construction archive and image evidence;
- successful install and removal preparation through official adapters;
- typed planner refusal retained without application or effect promotion;
- removal preparation proven independent of incoming artifact authority;
- explicit non-completed and indeterminate publication outcomes;
- CLI usage, authority-failure, and deterministic output contracts;
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
- proof that all exposed CLI commands remain read-only in 0.13.0;
- release, source, manual, shell, and patch-hygiene contracts.

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
- reinspection retains the construction archive digest, normalized image
  identity, and entry count without requiring one backend identity;
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
