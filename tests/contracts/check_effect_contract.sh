#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/effect.h"
source="$srcdir/src/effect.cpp"
classification="$srcdir/src/effect_application_classification.h"

[ -s "$header" ] || {
  echo 'missing effect-session public header' >&2
  exit 1
}
[ -s "$source" ] || {
  echo 'missing effect-session implementation' >&2
  exit 1
}
[ -s "$classification" ] || {
  echo 'missing application/effect classification boundary' >&2
  exit 1
}

for required in \
  'class effectful_operation_request final' \
  'class effectful_operation_session final' \
  'class transaction_effect_body_sink' \
  'class transaction_effect_driver' \
  'class transaction_effect_state_observer' \
  'class transaction_effect_publication_driver' \
  'class native_transaction_effect_publication_driver final' \
  'pkgapply::validate_target_mutation_lease' \
  'pkgstate::apply_adapter::project_completed_application' \
  'driver.execute_lifecycle' \
  'driver.apply_application' \
  'driver.publish_state' \
  'pkgctl/transaction-evidence/1' \
  'state_publication_indeterminate' \
  'application_resolution_required'; do
  grep -F -- "$required" "$header" "$source" >/dev/null || {
    echo "missing effect authority contract: $required" >&2
    exit 1
  }
done

for owner_outcome in \
  'precondition_refused' \
  'failed_before_target_mutation' \
  'failed_fully_recovered' \
  'failed_with_partial_effects' \
  'effects_visible_durability_unconfirmed' \
  'indeterminate'; do
  grep -F -- "$owner_outcome" "$classification" >/dev/null || {
    echo "missing owner application classification: $owner_outcome" >&2
    exit 1
  }
done

[ "$(grep -F -c 'case pkgapply::application_attempt_outcome::' "$classification")" -eq 7 ] || {
  echo 'application/effect classification does not cover the exact owner vocabulary' >&2
  exit 1
}

[ "$(grep -R -F -l --exclude='report.cpp' 'failed_with_partial_effects' "$srcdir/src" | wc -l)" -eq 1 ] || {
  echo 'owner application uncertainty classification is duplicated across controller sources' >&2
  exit 1
}

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
  if grep -F -- "$forbidden" "$header" "$source" >/dev/null 2>&1; then
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
