# Maintaining pkgctl

## Authority review

Before accepting a feature, identify which component owns every consumed or
produced fact. Reject changes that make `pkgctl` parse, infer, recompute, or
serialize a value already owned by another library.

Controller-owned policy must remain visibly separate from authority results.
Defaults that can remove packages, mutate filesystems, initialize state, or
publish state are prohibited.

## Dependency direction

The executable directly depends on the exact libraries whose public values it
uses. The internal `pkgctl-core` library is not installed and does not publish a
second package-management API.

A future effectful backend may depend on build, image, plan, apply, and state
adapters. Those dependencies must be introduced only with the exact effect
boundary and must not leak their semantics into request parsing or reporting.

## Compatibility policy

Historical `pkgman` compatibility belongs in an explicit translation frontend.
Native command and session semantics are not weakened to preserve ambiguous
legacy behavior.

## Release procedure

1. qualify every commit boundary against exact tagged dependency bundles;
2. run strict GCC and Clang builds and all controller tests;
3. run ASan and UBSan over the complete authority closure;
4. check shared and static linkage and direct dependency isolation;
5. verify CLI read-only behavior and missing-state refusal;
6. update release metadata and manuals together;
7. compare independently replayed trees and stable patch IDs;
8. tag signed releases only from a clean tree.
