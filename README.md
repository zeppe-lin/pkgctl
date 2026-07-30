# pkgctl

`pkgctl` is the Zeppe-Lin package control plane.

It coordinates sealed package authorities without reimplementing their
semantics. The project is original C++17 code licensed under
GPL-3.0-or-later and copyright Alexandr Savca.


Release 0.9.1 hardens the existing durable effect-attempt store. Encoding
version two publishes one immutable read-only snapshot and then atomically
advances a checksummed read-only head, which is the physical commit point.
Recovery opens only the exact self-contained snapshot selected by that head.
Each selected snapshot must carry the exact sequence derivable from its retained
effect evidence, and lease loss cannot conceal unresolved publication intent.
Exact retries are idempotent across both crash windows. Strict version-one
record-only histories remain readable through full semantic-chain validation.
Appending a successor establishes the version-two head; exact retry of the
latest legacy record rewrites that selected snapshot as version two before head
publication. The store is crash-consistent, not an anti-rollback anchor.


Release 0.9.0 established immutable transaction dispatch and in-flight
ownership:

```text
transaction_progress + bounded dispatch policy
        |
        v
canonical ready-unit reservation
        |
        v
reserved -> started -> completed
        |             \
        |              -> uncertain operation observation
        \
         -> released-unstarted
```

A caller-issued 32-byte nonce distinguishes each physical dispatch attempt.
`reserve_next()` chooses the first canonical ready unit allowed by explicit
construction/check capacities, one hard operation lane, graph-member exclusion,
and stop-after-terminal-failure containment. The returned `transaction_run`
retains every reservation for the lifetime of the run, so a nonce or graph unit
cannot be silently dispatched twice.

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

There are no effect-implying CLI commands in 0.9.1. The command frontend
executes no source acquisition, build, check, planner,
lifecycle, application, publication, or restart authority. The internal check
controller invokes execution only through an injected driver supplied by a
library client. Automatic execution, retry policy, adaptive scheduling, durable
run storage, transaction-wide rollback, journal discovery, compaction, garbage
collection, and effectful command policy remain outside this release.

## Authority

`pkgctl` owns command policy, authority-call sequencing, controller session
binding, deterministic dispatch reservation, in-flight ownership,
evidence-driven transaction progression, current-state epoch binding,
durable controller-attempt snapshots, conservative restart classification,
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
