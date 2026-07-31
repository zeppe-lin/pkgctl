# pkgctl

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.

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

This release does not select or reserve work. It creates no loop, thread,
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

There are no effect-implying CLI commands in 0.11.0. The command frontend
executes no source acquisition, build, check, planner,
lifecycle, application, publication, or restart authority. The library can execute one caller-selected dispatch only through an injected
driver and explicit stores. Automatic reservation, execution loops,
semantic-evidence discovery, resource recovery, retry policy, adaptive
scheduling, transaction-wide rollback, journal discovery, compaction, garbage
collection, and effectful command policy remain outside this release.

## Authority

`pkgctl` owns command policy, authority-call sequencing, controller session
binding, deterministic dispatch reservation, in-flight ownership,
evidence-driven transaction progression, current-state epoch binding,
durable controller-attempt snapshots, one-dispatch write-ahead execution,
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
