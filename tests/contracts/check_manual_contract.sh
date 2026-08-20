#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}

readme="$srcdir/README.md"
[ -s "$readme" ] || {
  echo "missing current product overview: $readme" >&2
  exit 1
}
for command in \
  'pkgctl catalog' \
  'pkgctl resolve' \
  'pkgctl transaction' \
  'pkgctl run' \
  'pkgctl build' \
  'pkgctl inspect-run' \
  'pkgctl inspect-effect'; do
  grep -F -- "$command" "$readme" >/dev/null || {
    echo "README omits current command surface: $command" >&2
    exit 1
  }
done
grep -F '`pkgctl run` is the sole effect-implying command in 0.35.0.' \
  "$readme" >/dev/null
for obsolete in \
  'The executable still exposes only:' \
  'There are no effect-implying CLI commands in 0.13.0.'; do
  if grep -F -- "$obsolete" "$readme" >/dev/null 2>&1; then
    echo "obsolete current command surface in README: $obsolete" >&2
    exit 1
  fi
done

for page in "$srcdir/man/pkgctl.1.scd" \
            "$srcdir/man/pkgctl_orchestration.7.scd"; do
  [ -s "$page" ] || {
    echo "missing manual source: $page" >&2
    exit 1
  }
done

grep -F 'Version 0.43.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.42.2' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.42.1' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.42.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.4' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.3' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.2' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.1' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.40.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.39.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.38.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*pkgctl* *run* _catalog-options_ _state-options_ _resolution-options_' \
  "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '# RUN AND BUILD OPTIONS' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*pkgctl* *build* _package_ [*--check*]' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*--artifact-root* _path_' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*--package-object-store* _path_' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*--build-root-view* _sha256_' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*--lifecycle-root-view* _sha256_' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*--operation-policy* _profile_' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.43.0 closes the installed-package resource side plane.' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.42.2 keeps build-subject necessity and dependency preference' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.42.1 requires libpkgbuild 3.0.3' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.42.0 separates execution-root identity from managed-target identity' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.41.0 separates application mutation mechanism' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.40.4 requires libpkgapply-posix 3.2.3' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.40.3 preserves subordinate application uncertainty' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.40.2 requires libpkgapply-posix 3.2.2' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.40.1 requires libpkgtransaction 4.1.0' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'TARGET-OPERATION POLICY AUTHORITY' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.40.0 makes the native target-operation policy one explicit controller-' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'PHASE-LOCAL CONSTRUCTION AND CHECK AUTHORITY' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'CONSTRUCTION AND CHECK PROCESS-DEATH RECOVERY' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.39.0 keeps build- and check-scoped package relations' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.38.0 gives durably started construction and check dispatches' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'PACKAGE BUILD FRONTEND' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.37.0 exposes *pkgctl build package*' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'BOUNDED NATIVE TRANSACTION COMMAND' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.35.0 exposes *pkgctl run*' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.21.0' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'Version 0.21.0' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F '*--converge-exact*' "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*pkgctl* *inspect-run* *--run-store* _path_ *--journal* _identity_' \
  "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F '*pkgctl* *inspect-effect* *--effect-store* _path_ *--attempt* _identity_' \
  "$srcdir/man/pkgctl.1.scd" >/dev/null
grep -F 'EXACT EFFECT-INSPECTION COMMAND' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'does not require write access and cannot' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'typed effect-journal diagnostics' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'DURABLE EFFECT-ATTEMPT INSPECTION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'pairs the exact retained' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Append remains' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.21.0 exposes only' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'EXACT RUN-INSPECTION COMMAND' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'does not require write access and cannot' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'The canonical state store is opened with *open_existing*' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'One target-mutation lease must remain held' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'TRANSACTION PROGRESSION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'does not choose among' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'TRANSACTION CHECK SESSION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'TRANSACTION DISPATCH' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'DURABLE RUN ADMISSION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Nonce-source refusal performs no store write' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'DURABLE TRANSACTION-RUN INSPECTION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'quiescent-incomplete' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Inspection does not rehydrate package semantics' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'RESTART-SAFE TRANSACTION LAUNCH' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'append is attempted.' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'resumes the current head; retrying after completion returns completed through' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'DURABLE CONSTRUCTION AND CHECK EVIDENCE' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.28.0 commits construction and check evidence' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'NATIVE CONSTRUCTION AND CHECK SESSION LOCATION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.32.0 implements one store-backed' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Exact transaction-progress rehydration is the next' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'SHARED CONSTRUCTION AND CHECK SESSION AUTHORITY' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.30.0 makes one deterministic session source authoritative' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'pure request projections' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'EVIDENCE-BACKED CONSTRUCTION AND CHECK RECOVERY' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Version 0.29.0 composes one exact durable construction/check record' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Missing evidence means a' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'The stored identity is never promoted into semantic' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'SINGLE-DISPATCH EXECUTION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'BOUNDED SERIAL TRANSACTION DRIVE' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Exact retries against the same record must' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'outcome may follow a completed, stopped, externally blocked, or quiescent one.' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'ONE-STEP TRANSACTION ADVANCEMENT' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Recovery therefore has precedence over every' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'reservation is committed before fresh execution' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'One call either reconciles retained ownership or executes one newly' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'EXACT RUN-AUTHORITY REHYDRATION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Restart evidence is therefore exact and not' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'DURABLE RESTART RECONCILIATION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'A terminal effect record can therefore repair a' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'external resolution invokes no driver' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'A failed start append invokes no driver' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'DURABLE TRANSACTION RUNS' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'head is the physical commit point' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'stored identity is never promoted into semantic evidence' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'effect-attempt admission first and the started run snapshot second' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'the exact authority returned by both stores. Only after both commits may an' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'one non-configurable mutation lane' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Already-started independent work' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'released-unstarted' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'independent ready work may advance concurrently' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'OPERATION PREPARATION' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'typed refusal' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'consumes the complete *libpkgbuild-image* authority' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Preparation reads no artifact bytes and performs no archive I/O.' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'CANDIDATE CONSTRUCTION' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'independent archive inspection' "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'DURABLE EFFECT ATTEMPTS' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'exact durable *libpkgapply* journal' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'newly held physical target-mutation lease' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null

for obsolete in \
  'reinspects the retained artifact' \
  'Preparation may read exact artifact bytes' \
  'artifact reinspection'; do
  if grep -R -n -F "$obsolete" "$srcdir/man" >/dev/null 2>&1; then
    echo "obsolete artifact-reinspection authority in manuals: $obsolete" >&2
    exit 1
  fi
done

for obsolete in 'forbid-node' 'operation graph ordering' 'download named'; do
  if grep -R -n -F "$obsolete" "$srcdir/man" >/dev/null 2>&1; then
    echo "obsolete provisional controller semantic in manuals: $obsolete" >&2
    exit 1
  fi
done

if grep -nE '^[1-9][0-9]*\. ' "$srcdir"/man/*.scd >/dev/null 2>&1; then
  echo 'ordered scdoc lists must use dot-item markup' >&2
  exit 1
fi

# Installed package bytes are present resource authority, not caller-authored
# tree mappings or a reason to rediscover package semantics.
grep -F 'INSTALLED PACKAGE RESOURCE PREPARATION' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
grep -F 'Missing or corrupt exact' \
  "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null
if grep -F -x -- '*--installed-tree* _package=resource,path_' "$srcdir/man/pkgctl.1.scd" >/dev/null 2>&1; then
  echo 'manual still advertises caller-authored installed-tree authority' >&2
  exit 1
fi
