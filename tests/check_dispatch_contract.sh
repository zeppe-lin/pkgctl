#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
header="$srcdir/include/pkgctl/dispatch.h"
source="$srcdir/src/dispatch.cpp"
progression="$srcdir/include/pkgctl/progression.h"

for file in "$header" "$source" "$progression"; do
  [ -s "$file" ] || {
    echo "missing transaction dispatch authority source: $file" >&2
    exit 1
  }
done

for required in \
  'class transaction_dispatch_nonce final' \
  'class transaction_dispatch_policy final' \
  'class transaction_dispatch_dependency final' \
  'class transaction_dispatch final' \
  'class transaction_dispatch_record final' \
  'class transaction_run final' \
  'transaction_dispatch_state::reserved' \
  'transaction_dispatch_state::started' \
  'transaction_dispatch_state::completed' \
  'transaction_dispatch_state::released_unstarted' \
  'reserve_next' \
  'start_construction_dispatch' \
  'start_check_dispatch' \
  'start_operation_dispatch' \
  'operation_dispatch_start_result' \
  'effect_attempt_record::admit' \
  'effect_attempt() const noexcept' \
  'started dispatch has invalid effect-attempt authority' \
  'release_unstarted_dispatch' \
  'complete_construction_dispatch' \
  'complete_check_dispatch' \
  'submit_operation_dispatch_result' \
  'capture_dependencies' \
  'validate_construction_inputs' \
  'validate_exact_check_construction' \
  'operation_capacity() const noexcept' \
  'return 1U;' \
  'failure containment forbids starting reserved work' \
  'operation dispatch was reserved against a stale state epoch' \
  'indeterminate operation observation cannot advance state'; do
  grep -F "$required" "$header" "$source" >/dev/null || {
    echo "missing transaction dispatch contract: $required" >&2
    exit 1
  }
done

for forbidden in \
  'execute_construction(' \
  'execute_transaction_check(' \
  'execute_effectful_operation(' \
  'pkgexec::execute' \
  'pkgapply::apply' \
  'compare_and_publish' \
  'canonical_generation_store' \
  'libpkgexec-linux' \
  'std::thread' \
  'std::async' \
  'sleep(' \
  'pkgmk' \
  'pkgman'; do
  if grep -F "$forbidden" "$header" "$source" >/dev/null 2>&1; then
    echo "forbidden transaction dispatch shortcut: $forbidden" >&2
    exit 1
  fi
done

if grep -R -n -E \
    'transaction_run|transaction_dispatch|reserve_next' \
    "$srcdir/cli" >/dev/null 2>&1; then
  echo 'transaction dispatch must not acquire a command frontend' >&2
  exit 1
fi
