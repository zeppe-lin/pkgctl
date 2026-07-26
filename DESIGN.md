# pkgctl design

## Purpose

`pkgctl` is the orchestration authority for package transactions in Zeppe-Lin.
It coordinates independent source, build, image, planning, application, and
state authorities without absorbing their semantics.

The central invariant is:

> pkgctl decides what authority to call next, but it does not recreate the
> authority's facts or decisions.

## Owned semantics

The native model owns only orchestration concerns:

1. user intent;
2. typed transaction constraints;
3. package-operation graph selection;
4. execution ordering and continuation policy;
5. per-step orchestration outcomes;
6. transaction progress and recovery routing;
7. stable diagnostics and presentation.

The model is deliberately independent of external command exit-code tables,
archive naming conventions, source-directory formats, filesystem mutation
rules, and installed-state serialization.

## Non-authorities

The following meanings remain external:

| Meaning | Authority |
| --- | --- |
| source directory capture and normalization | `libpkgsource` |
| build execution and exact artifact result | `libpkgbuild` |
| archive inspection and normalized package image | `libpkgimage` |
| canonical installed state | `libpkgstate` |
| installed-state projection into planner facts | `libpkgstate-plan` |
| one-package install, upgrade, and removal policy | `libpkgplan` |
| physical application and recovery evidence | `libpkgapply` |
| completed application projection into state publication | `libpkgstate-apply` |

`pkgctl` must never create a second implementation of one of these meanings.

## Clean-room rule

The project begins from an empty licensed repository. Source code from `pkgman`
or another package manager is not copied, mechanically translated, or used as a
class-layout template.

Compatibility research may record:

- public commands and options;
- configuration accepted by deployed systems;
- output consumed by scripts;
- documented and observed behavior;
- migration requirements and failure cases.

Compatibility research must not preserve:

- internal class names or inheritance structure;
- transaction result enums;
- resolver algorithms or control flow;
- package-database parsers;
- artifact-path reconstruction rules;
- process-management implementation.

## Transaction shape

A complete future one-package transaction has this authority sequence:

```text
canonical state snapshot
        |
        v
planner state projection + candidate facts + inspected image
        |
        v
immutable package plan or typed refusal
        |
        v
exact application request
        |
        v
completed application evidence or recovery state
        |
        v
state publication request
        |
        v
stale-safe compare-and-publish receipt
```

A multi-package transaction composes these operations through an immutable DAG.
It does not claim global atomicity across package builds, filesystem effects,
and canonical state publication.

## Root semantics

There is no implicit global alternate root. A future target context must bind
separate target, state-store, build-environment, application-backend, and
lifecycle-environment identities.

Until that complete contract exists, the supported execution mode is the host
target with canonical state bound to that target. Unsupported cross-root
requests must fail explicitly.

## Compatibility

`pkgman` remains a migration-era compatibility program. Native `pkgctl` commands
and options are not required to preserve ambiguous legacy names.

Compatibility aliases, if implemented, live in a narrow frontend and translate
into native intent and constraint values. They do not alter the native model.

## Release 0.1.0

The first release establishes:

- the clean-room project and license;
- the internal testable orchestration library;
- typed intent and constraints;
- orchestrator-native step outcomes;
- immutable validated operation graphs;
- a non-operational CLI exposing only help and version.

It performs no source inspection, building, archive opening, planning,
application, state publication, lifecycle execution, or recovery.
