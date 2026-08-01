# pkgctl

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

Release 0.21.0 exposes exact durable effect-attempt inspection on the read-only
command surface:

```sh
pkgctl inspect-effect --effect-store /path/to/effect-store --attempt SHA256
```

The command opens only the explicitly named existing POSIX store and loads only
the explicitly named attempt. It delegates classification to
`inspect_effect_attempt()` and output to `render_report()`; it does not scan for
attempts, traverse run journals, rehydrate subordinate semantics, construct
restart authority, invoke a driver, append, reconcile, repair, or mutate
anything. Effect-journal failures retain their typed diagnostics, and read-only
store access does not create or open the writer lock for writing.

Release 0.20.0 established the underlying durable sensor. It pairs one
storage-derived `effect_attempt_record` with the existing pure
`effect_restart_assessment` and reports controller-owned record, predecessor,
lifecycle, application, transaction, publication, terminal, and reconciled
state identities without promoting any identity into semantic evidence.

Release 0.19.0 exposes exact durable transaction-run inspection on the
read-only command surface:

```sh
pkgctl inspect-run --run-store /var/lib/pkgctl/runs --journal SHA256
```

The command opens only the explicitly named existing POSIX store and loads only
the explicitly named journal. It delegates classification to
`inspect_transaction_run()` and output to `render_report()`; it does not scan for
runs, infer semantic progression, inspect effect journals, append, reserve,
execute, repair, or mutate anything. Read-only store access does not create or
open the writer lock for writing.

Release 0.18.0 established the underlying durable sensor. It classifies one
storage-derived head as completed, stopped after failure, active, or quiescent
incomplete and retains exact restart dispositions without promoting identities
into semantic evidence.

Release 0.17.0 establishes restart-safe transaction launch:

```text
exact initial progress + policy
              |
              v
   replay-safe run nonce
              |
              v
 derive exact journal identity
              |
      +-------+--------+
      |                |
 no committed head   existing exact head
      |                |
 append sequence zero  resume it
      +-------+--------+
              |
              v
      bounded serial drive
```

`launch_transaction_run()` composes admission and bounded driving without
replaying admission over an advanced journal. It derives the exact journal from
the immutable initial run and caller-owned run nonce, loads that journal head,
and appends sequence zero only when no committed head exists. An existing head
must retain the same transaction, run nonce, and dispatch policy before it may
be resumed.

A failure before the admission append performs no drive action. A failure after
admission leaves sequence zero or a later ownership record durable for an exact
retry. Retrying after partial or completed work loads the current head and does
not republish sequence zero, consume fresh dispatch authority for terminal work,
or invoke a completed driver again. The launch remains explicitly bounded by
`transaction_run_drive_policy`.

Release 0.17.0 adds no journal discovery, unbounded execution, worker,
concurrency, adaptive scheduling, retry timing, backoff, resource or evidence
discovery, process adoption, rollback, cleanup, compaction, garbage collection,
or mutating CLI command.

Release 0.16.0 establishes the durable origin of one transaction-run history:

```text
exact initial progress + dispatch policy
                |
                v
       immutable initial run
                |
                v
 replay-safe caller nonce authority
                |
                v
      sequence-zero record
                |
                v
       durable store append
                |
                v
 storage-derived reopened run
```

`admit_transaction_run()` constructs one exact initial run, obtains a run nonce
from an injected `transaction_run_nonce_source`, commits sequence zero, validates
the authority returned by storage, and reopens the run from that committed
record. Exact retries for the same initial run must receive the same nonce and
converge on the same record. A nonce-source refusal performs no store write; a
store failure grants no controller authority.

Admission reserves no unit, acquires no execution resources, invokes no driver,
and performs no effect-journal operation. It adds no scheduler, drive loop,
retry timing, discovery, rollback, cleanup, compaction, garbage collection, or
mutating CLI command.

Release 0.15.0 adds an explicitly bounded serial transaction drive without
turning the control plane into a scheduler:

```text
committed run head
        |
        v
reconcile retained ownership, if any
        |
        `-- otherwise issue nonce for this exact head
                         |
                         v
                 reserve and execute one
                         |
                         v
                 classify durable outcome
                         |
            +------------+-------------+
            |                          |
        stop condition             bound remains
            |                          |
          return                 reload head
```

`drive_transaction_run()` accepts a positive maximum step count and repeatedly
invokes the one-step authority. Every iteration reloads the committed head.
Completion, containment failure, external-resolution authority, incomplete
quiescence, or exhaustion of the explicit bound stops the call.

Fresh dispatch nonces come from an injected
`transaction_dispatch_nonce_source` keyed to the storage-derived record and
rehydrated run. The source is not called while retained ownership is being
reconciled or when the run is stopped, complete, or otherwise has no ready work.
Exact retries against an unchanged head must return the same nonce; a committed
successor head may yield a different nonce. This prevents speculative nonce
preallocation from consuming attempt identity outside durable ownership.

The returned drive result retains the ordered one-step outcomes and validates
that continued steps remain in one journal, follow strictly increasing durable
heads, and never continue after a stopping result. Release 0.15.0 adds no
worker, concurrency, adaptive priority, unbounded loop, retry timing, backoff,
resource or evidence discovery, rollback, cleanup policy, compaction, garbage
collection, or mutating CLI command.

Release 0.14.0 composes the durable run, exact authority, execution, and
reconciliation boundaries into one bounded transaction advancement:

```text
committed run head
        |
        v
exact progression rehydration
        |
        +-- retained ownership --> reconcile one dispatch --> return
        |
        `-- quiescent -----------> reserve one dispatch
                                      |
                                      v
                              commit reservation
                                      |
                                      v
                              acquire exact authority
                                      |
                                      v
                              execute and commit --> return
```

`advance_transaction_run_once()` loads the committed head selected by an exact
journal identity. It rehydrates semantic progression through the caller-owned
source and gives the first active dispatch in durable ledger order absolute
precedence. Reserved ownership is released; started construction or check work
accepts exact recovered evidence; started operation work inspects the retained
effect attempt. External-resolution effect state returns without a driver call
or journal append. No fresh work is reserved in the same call.

Only a quiescent reopened run may reserve the first canonical ready dispatch.
The selected driver and effect-store requirements are validated before the
reservation append. The reservation is then committed before fresh execution
authority is requested, and the existing write-ahead execution functions retain
start and terminal crash barriers. An authority-source or driver failure therefore
leaves exact reserved or started ownership rather than an invisible partial step.

The returned result is reconstructed from storage-derived run authority and
validates its disposition, retained dispatch, and semantic evidence. Release
0.14.0 adds no loop, scheduler, worker, concurrency, retry or backoff policy,
resource or evidence discovery, process adoption, rollback, cleanup policy,
compaction, garbage collection, or effectful CLI command.

Release 0.13.0 closes the authority handoff required before a deterministic
transaction runner can exist:

```text
durable run identity
        |
        v
caller-owned progression/resource/recovery source
        |
        v
exact pkgctl validation
        |
        v
sealed execution or recovery handoff
```

`rehydrate_transaction_run()` obtains one complete semantic progression from an
injected source and reopens only the exact run named by the durable record.
`acquire_transaction_dispatch_execution_authority()` obtains fresh concrete
construction, check, or operation resources for one exact reserved dispatch and
validates them through the existing pure start transition. It stores nothing and
invokes no driver.

`acquire_transaction_dispatch_recovery_authority()` obtains exact subordinate
evidence for one restart-classified active dispatch. Started construction and
check evidence must retain the exact admitted session already named by the run.
Operation recovery must retain both the exact effect session and effect-attempt
identity. A never-started reservation requires no evidence source call.

Fresh resources may vary where their owning admission contract declares paths or
other coordinates non-semantic. Restart evidence may not vary: it must identify
the exact durable attempt. Release 0.13.0 adds no journal access, reservation,
execution, reconciliation, evidence discovery, scheduler, loop, retry policy,
rollback, compaction, garbage collection, or effectful CLI command.

Release 0.12.0 closes the durable restart actuator for one exact dispatch:

```text
committed run restart checkpoint
        |
        v
validate caller-rehydrated recovery authority
        |
        v
release reserved work | accept build/check evidence | continue effect journal
        |
        v
commit one exact run successor or stop for external resolution
```

`reconcile_reserved_dispatch_durable()` durably releases only a dispatch that
restart classified as reserved and never started.
`reconcile_construction_dispatch_durable()` and
`reconcile_check_dispatch_durable()` accept caller-rehydrated terminal evidence
only when its admitted session is the exact session retained by started
ownership. They submit through the existing completion functions and commit one
validated run successor. A lost final append is exactly retryable.

`reconcile_operation_dispatch_durable()` verifies the run-retained effect
attempt against the latest exact effect-journal record. Automatically
continuable stages resume through the existing effect restart authority. A
terminal effect journal can repair a lost run completion without invoking the
mutation driver again. Stages requiring external resolution advance neither
journal nor run and invoke no driver. Successful continuation rereads canonical
installed state before progression accepts the result.

The journal still does not reconstruct semantic evidence. Construction and check
results, effect restart checkpoints, drivers, leases, resources, and stores are
caller-supplied exact authorities. Release 0.12.0 adds no work selection,
scheduler, execution loop, evidence discovery, retry timing, process adoption,
rollback, compaction, garbage collection, or effectful CLI command.

Release 0.11.0 closes one explicitly selected dispatch behind the durable
transaction-run barrier:

```text
reserved run snapshot
        |
        v
commit started ownership
        |
        v
invoke one injected driver
        |
        v
commit terminal evidence or retained uncertainty
```

`execute_construction_dispatch_durable()` and
`execute_check_dispatch_durable()` commit the exact started-run successor before
their driver is invoked. They then submit the existing terminal controller
evidence through the ordinary dispatch completion functions and commit that one
successor. A failed start append invokes no driver. Driver escape or failed
terminal commitment fabricates no completion: the committed started dispatch
remains the restart authority.

`execute_operation_dispatch_durable()` preserves the stricter cross-journal
order. It commits effect-attempt admission, commits the started run retaining
that attempt, executes through the existing durable effect journal, and only
then commits the resulting run successor. Successful publication is paired with
a fresh authoritative state read. Lost-lease and indeterminate-publication
results remain ordered observations on an active dispatch. If the effect journal
reaches terminal state but the final run append is lost, restart still names the
exact effect journal to inspect.

Release 0.11.0 does not select or reserve work. It creates no loop, thread,
backend, resource-discovery policy, retry policy, scheduler, transaction-wide
cancellation, rollback, or effectful CLI command. The caller supplies one exact
reservation, admitted session, driver, and both stores.

Release 0.10.0 established durable transaction-run ownership and conservative
restart classification:

```text
exact transaction_progress + immutable dispatch ledger
        |
        v
immutable, single-transition run snapshots
        |
        v
checksummed committed head
        |
        v
exact progression rehydration
        |
        v
release reserved work | recover build/check | inspect effect journal
```

A durable run history begins before the first reservation, so sequence zero
contains no dispatch ownership and every positive sequence retains at least
one reservation. Its sequence must equal the transition count derivable from
retained dispatch states and uncertainty observations. Every successor snapshot
contains exactly one legal ledger transition: one reservation, one start, one
unstarted release, one uncertainty observation, or one terminal completion. A
reservation successor must bind the predecessor's exact progression identity
and state epoch. Records bind the transaction, dispatch policy, caller-issued
run nonce, sequence, predecessor, current state epoch, progression identity, and
complete immutable dispatch history. The POSIX store publishes immutable record
files and then atomically advances a checksummed read-only head. Recovery opens
only the exact self-contained snapshot named by that committed head; exact crash
retries are idempotent.

The journal does not serialize package truth. After restart, the caller must
rehydrate the exact `transaction_progress` from authoritative construction,
check, effect, and state evidence. `reopen()` verifies that authority and then
restores dispatch ownership. A digest in a journal record is never promoted into
a build result, check result, effect result, or installed-state snapshot.

Restart classification is conservative. A durably `reserved` dispatch may be
released because no execution session was admitted. A durably `started`
construction or check requires external recovery evidence. A started operation
retains the exact effect-attempt identity whose journal must be inspected.
`commit_operation_dispatch_start()` commits that effect admission first and the
started run snapshot second. Only after both commits may an effect driver run.
An interrupted exact call can be retried; an orphan admission does not promote a
still-reserved dispatch into started work.

Release 0.9.1 established the committed-head protocol for the effect-attempt
store. A head-selected snapshot must carry the exact sequence derivable from its
retained effect evidence and cannot represent lease loss over an unresolved
publication intent. Release 0.10.0 reuses that lower commit point rather than
defining a second interpretation of effect durability.

Release 0.9.0 established immutable transaction dispatch and in-flight
ownership. A caller-issued 32-byte nonce distinguishes each physical dispatch
attempt. `reserve_next()` chooses the first canonical ready unit allowed by
explicit construction/check capacities, one hard operation lane,
graph-member exclusion, and stop-after-terminal-failure containment. The
returned `transaction_run` retains every reservation for the lifetime of the
run, so a nonce or graph unit cannot be silently dispatched twice.

Reservation is not execution. A unit becomes `started` only after an exact
construction, check, or operation session is admitted against the retained
transaction and predecessor evidence. Unstarted work can be released. Started
work cannot be called unstarted again. Construction and check completion retire
only the exact started session. Definitive operation evidence retires the
operation; lost-lease and indeterminate-publication evidence remains an active
observation until authoritative resolution arrives.

The ledger binds exact successful predecessor evidence. Check-scoped package
inputs are ordered before construction by `libpkgtransaction >= 2.1.0`, then
verified against the retained build result and artifact before a construction
session starts. Retained installed inputs must still be the same exact package
in the current state. Failure containment prevents both new reservations and
starting merely reserved work after terminal failure, while already-started
independent work may still submit exact terminal evidence.

Release 0.8.0 closed exact check request, concrete session, execution, and
terminal progression evidence. Dispatch composes those existing boundaries; it
does not invoke them automatically.

Release 0.7.1 established evidence-driven transaction progression and
current-state epochs. A target action and its exact pre- and post-lifecycle
phase nodes form one operation unit, while build and check nodes remain exact
individual units. Independent ready units remain visible together; `pkgctl`
does not choose among them.

Release 0.6.0 established one-operation preparation between completed
construction and the target-effect kernel. Release 0.5.0 established one exact
package-construction session, and 0.4.0 closed the restart loop for one target
mutation sequence.

The executable still exposes only:

```text
pkgctl catalog
pkgctl resolve
pkgctl transaction
```

Every collection root, target-state binding identity, architecture, goal scope,
and destructive convergence choice is explicit. `transaction` defaults to
`preserve-unselected`; exact convergence requires `--converge-exact`.

There are no effect-implying CLI commands in 0.13.0. The command frontend
executes no source acquisition, build, check, planner, lifecycle, application,
publication, or restart authority. The library can execute or reconcile one
caller-selected dispatch only through injected drivers, exact checkpoints, and
explicit stores. Automatic reservation, execution loops,
semantic-evidence discovery, resource recovery, retry policy, adaptive
scheduling, transaction-wide rollback, journal discovery, compaction, garbage
collection, and effectful command policy remain outside this release.

## Authority

`pkgctl` owns command policy, authority-call sequencing, controller session
binding, deterministic dispatch reservation, in-flight ownership,
evidence-driven transaction progression, current-state epoch binding,
durable controller-attempt snapshots, one-dispatch write-ahead execution,
exact one-dispatch restart reconciliation, exact run-authority rehydration,
conservative restart classification,
outer-lease observation, transaction-evidence composition, deterministic
diagnostics, and presentation.

The following meanings remain external:

- recipe and profile syntax: `libpkgsource-yaml`;
- available package universe: `libpkgcatalog` and
  `libpkgcatalog-acquire`;
- installed truth, planner projection, and publication: `libpkgstate`,
  `libpkgstate-plan`, and `libpkgstate-apply`;
- exact dependency selection: `libpkgresolve`;
- cross-package operation composition: `libpkgtransaction`;
- pure check requests and terminal evidence: `libpkgcheck`;
- concrete check execution admission and realization: `libpkgcheck-exec`;
- candidate and artifact planner projection: `libpkgsource-plan` and
  `libpkgbuild-plan`;
- package-local filesystem intent and mutation: `libpkgplan` and
  `libpkgapply`;
- lifecycle-node derivation and execution: `libpkgapply-exec` and
  `libpkgexec`;
- source acquisition and verification: `libpkgfetch`;
- build request/result and execution realization: `libpkgbuild` and
  `libpkgbuild-exec`;
- package-image authority: `libpkgimage`.

`transaction_progress` does not infer transaction order beyond the exact
transaction graph and exposes every ready unit. The dispatch ledger may reserve
the first canonical ready unit allowed by explicit capacity and failure policy;
it does not invent graph edges or execute the reservation. For lifecycle nodes
not ordered relative to each other by that graph, the caller supplies an exact
order and the controller binds it into the effect request identity.

## Building

```sh
meson setup build \
  -Dlink_mode=shared \
  -Dman_pages=enabled
meson compile -C build
meson test -C build --print-errorlogs
```

Use `-Dlink_mode=static` for a static authority closure where all external
static dependencies are available.

See `DESIGN.md` for the normative controller boundary and `TESTING.md` for the
qualification contract.
