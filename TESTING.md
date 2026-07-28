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
- explicit non-completed and indeterminate publication outcomes;
- CLI usage, authority-failure, and deterministic output contracts;
- proof that all exposed CLI commands remain read-only in 0.3.0;
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

## Negative authority tests

Tests must also reject:

- empty collection or resolution requests;
- relative canonical-store paths;
- duplicate semantic goals;
- missing target-binding options;
- missing or mismatched canonical stores;
- transaction-only policy on another command;
- unknown goal scopes, lifecycle actions, and collection revisions;
- multi-package or runtime-cohort effect programs in the initial effect boundary;
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
