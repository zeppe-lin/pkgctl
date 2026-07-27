# pkgctl testing

The suite protects authority composition rather than implementation shape.

## Required qualification

Every release must establish:

- strict C++17 compilation under GCC and Clang;
- independent usability of every internal public header;
- explicit request validation and duplicate-goal rejection;
- deterministic authority and session identities;
- path and diagnostic-provenance independence where non-semantic;
- exact existing-state binding and read-only store access;
- catalog, resolution, and transaction composition against exact tagged
  dependency sources;
- typed build, run, check, and lifecycle goal handling;
- installed-versus-catalog and convergence-policy reporting;
- CLI usage, authority-failure, and deterministic output contracts;
- proof that successful commands do not alter the canonical state store;
- release, source, manual, shell, and patch-hygiene contracts.

## Negative authority tests

Tests must reject:

- empty collection or resolution requests;
- relative canonical-store paths;
- duplicate semantic goals;
- missing target-binding options;
- missing or mismatched canonical stores;
- transaction-only policy on another command;
- unknown goal scopes, lifecycle actions, and collection revisions;
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
