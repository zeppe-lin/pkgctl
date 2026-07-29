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
- simultaneous exposure of independent ready units without controller
  selection;
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
- strict codec, predecessor-chain, causal-shape, and POSIX store validation;
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
- proof that all exposed CLI commands remain read-only in 0.8.0;
- release, source, manual, shell, and patch-hygiene contracts.

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
