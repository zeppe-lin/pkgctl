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

The preparation layer may depend on the published state-plan, source-plan,
build-plan, planner, image, and application values. It may inspect exact artifact
bytes through an injected backend, but must not observe target paths itself,
normalize package policy, discover runtime closure, execute lifecycle programs,
mutate the target, or publish state.

The check layer may depend on transaction progression, construction evidence,
`libpkgcheck`, and `libpkgcheck-exec`. Pure request admission must remain usable
before host paths exist. Concrete resources, interpreter identity, credentials,
and limits belong only to the admitted check session. The controller must not
reimplement input-tree matching, resource-layout construction, process-status
classification, or backend execution.

The pure dispatch layer may depend on transaction progression and exact admitted
construction, check, and effect sessions. It owns deterministic reservation,
bounded in-flight capacity, caller attempt nonces, and immutable ownership
records. It must not execute a driver, create a backend, allocate resource paths,
construct new transaction edges, or infer success from reservation state.
Check-scoped package inputs require `libpkgtransaction >= 2.1.0` so exact input
authority precedes the construction that seals it.

The single-dispatch execution layer may compose the pure dispatch functions with
explicit run/effect stores and an injected driver. It must commit started
ownership before driver invocation and commit only evidence accepted by the
existing completion functions. Start failure invokes no driver. Driver or final
commit failure leaves started ownership durable. This layer must not reserve
work, loop, discover resources, create backends, choose retries, or release work
automatically.

The restart-reconciliation layer may consume one exact
`transaction_run_restart_checkpoint`, one retained dispatch, and explicit
caller-rehydrated subordinate authority. Reserved release must require the
`release_reserved` disposition. Construction and check recovery must validate
the exact retained attempt session before storage. Operation recovery must bind
the run-retained effect attempt to the latest supplied effect-journal record and
delegate continuation to `resume_effectful_operation()`. External-resolution
stages invoke no driver and append no run successor. This layer must not scan for
evidence, construct a result from an identity, discover resources or journals,
adopt processes, choose retry timing, reserve work, or loop.

The durable run-journal layer may serialize only controller-owned dispatch
ownership, exact identities, causal sequence, and terminal flags. Reopening must
consume an exact rehydrated `transaction_progress` and revalidate graph units,
predecessor evidence, completed evidence, and active operation state. It must
not deserialize an identity into subordinate semantic evidence. A started
transition must be appended and synchronized before its driver is invoked;
append failure is a stop condition, not permission to execute without durable
ownership. Operation start is stricter: commit the exact effect-attempt
admission first, then the started run snapshot retaining that attempt identity.
The ordered pair must remain exactly retryable, and an orphan admission must
never be interpreted as started target mutation. POSIX changes must preserve
the two-stage commit: immutable record
publication and synchronization first, checksummed head replacement and final
directory synchronization second. Exact retries must be idempotent, while a
foreign same-name record must fail closed. Do not restore operational loading by
scanning every full historical snapshot.

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
   check request/session/result binding, canonical multi-input projection,
   concurrency-safe check progression, deterministic dispatch reservation,
   exact predecessor and state-epoch binding, operation-lane serialization,
   failure containment, durable run single-transition sealing, exact progression
   rehydration, graph/evidence revalidation, write-ahead start persistence,
   one-dispatch driver barriers, exact reserved release, caller-rehydrated
   build/check recovery, effect-journal continuation, lost-terminal-write
   recovery, preparation projection and typed refusal, effect sequencing,
   intent-before-effect persistence, exact restart checkpoints, outer-lease
   reacquisition, publication reconciliation, publication provenance, CLI
   read-only behavior, and missing-state refusal;
6. update release metadata and manuals together;
7. compare independently replayed trees and stable patch IDs;
8. tag signed releases only from a clean tree.
