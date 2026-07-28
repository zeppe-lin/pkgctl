#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/effect.h"
source="$srcdir/src/effect.cpp"

[ -s "$header" ] || {
  echo 'missing effect-session public header' >&2
  exit 1
}
[ -s "$source" ] || {
  echo 'missing effect-session implementation' >&2
  exit 1
}

for required in \
  'class effectful_operation_request final' \
  'class effectful_operation_session final' \
  'class transaction_effect_driver' \
  'pkgapply::validate_target_mutation_lease' \
  'pkgstate::apply_adapter::project_completed_application' \
  'driver.execute_lifecycle' \
  'driver.apply_application' \
  'driver.publish_state' \
  'pkgctl/transaction-evidence/1' \
  'state_publication_indeterminate'; do
  grep -F "$required" "$header" "$source" >/dev/null || {
    echo "missing effect authority contract: $required" >&2
    exit 1
  }
done

held_checks=$(grep -F -c 'driver.lease().held()' "$source")
[ "$held_checks" -ge 5 ] || {
  echo 'effect sequence does not observe the outer lease at every boundary' >&2
  exit 1
}

for forbidden in \
  'libpkgexec-linux' \
  'canonical_generation_store::initialize' \
  'pkgmk' \
  'pkgman'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden effect-layer authority dependency: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'execute_effectful_operation|native_transaction_effect_driver' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'effect command frontend must remain read-only' >&2
  exit 1
fi
