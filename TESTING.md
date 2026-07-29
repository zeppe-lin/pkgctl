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
- exact action/construction/state binding before package-local planning;
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
- proof that all exposed CLI commands remain read-only in 0.6.0;
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
