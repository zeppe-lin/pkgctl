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

The construction layer may depend on source, fetch, build, build-exec, image,
and backend-neutral execution authorities, but must not schedule dependency
graphs or construct a Linux backend.

The effectful controller layer may depend on image, plan, apply, execution,
and state adapters only through their exact public values. It must not parse
payloads, derive lifecycle programs, perform target mutation, or serialize
installed state itself. Physical effects stay behind injected authority
interfaces, and the CLI remains separate from the effect-session kernel.

The controller journal is not an alternate application or state store. New
restart paths must consume exact subordinate journals and receipts, never scan
for them, infer them from names, or overwrite ambiguous evidence. A restarted
attempt always proves a newly held physical mutation lease.

## Compatibility policy

Historical `pkgman` compatibility belongs in an explicit translation frontend.
Native command and session semantics are not weakened to preserve ambiguous
legacy behavior.

## Release procedure

1. qualify every commit boundary against exact tagged dependency bundles;
2. run strict GCC and Clang builds and all controller tests;
3. run ASan and UBSan over the complete authority closure;
4. check shared and static linkage and direct dependency isolation;
5. verify construction authority binding, source/build evidence retention,
   effect sequencing, intent-before-effect persistence, exact restart
   checkpoints, outer-lease reacquisition, publication reconciliation,
   publication provenance, CLI read-only behavior, and missing-state refusal;
6. update release metadata and manuals together;
7. compare independently replayed trees and stable patch IDs;
8. tag signed releases only from a clean tree.
