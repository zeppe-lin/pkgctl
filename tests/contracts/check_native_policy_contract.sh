#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=${1:-.}
header=$root/include/pkgctl/native_policy.h
source=$root/src/native_policy.cpp
options=$root/cli/options.cpp
command=$root/cli/run_command.cpp
operation_header=$root/include/pkgctl/run_operation.h
operation_codec=$root/src/operation_session_codec.cpp
unit=$root/tests/unit/native_policy_test.cpp
readonly=$root/tests/integration/cli_readonly_test.sh
native_construction=$root/tests/integration/cli_run_native_construction_test.sh

for file in \
  "$header" "$source" "$options" "$command" "$operation_header" \
  "$operation_codec" "$unit" "$readonly" "$native_construction"; do
  [ -s "$file" ] || {
    echo "missing native operation policy authority file: $file" >&2
    exit 1
  }
done

for required in \
  'enum class native_operation_policy_profile' \
  'strict_exclusive = 1' \
  'exact_compatible_sharing = 2' \
  'class native_operation_policy final' \
  'native_operation_policy::seal' \
  'pkgctl/native-operation-policy/1' \
  'strict-exclusive/v1' \
  'exact-compatible-sharing/v1' \
  'incoming=activate;obsolete=remove;shared=forbid;' \
  'incoming=activate;obsolete=remove;shared=allow-compatible;' \
  'directory-cleanup=remove-if-empty;overrides=none' \
  'pkgplan::incoming_path_policy::activate()' \
  'pkgplan::obsolete_path_policy::remove()' \
  'pkgplan::directory_cleanup_policy::remove_if_empty' \
  'encode_native_operation_policy' \
  'decode_native_operation_policy' \
  'native operation policy identity contradicts retained profile' \
  'encode_native_operation_policy(decoded) != encoding'; do
  grep -F -- "$required" "$header" "$source" >/dev/null || {
    echo "native operation policy owner omits contract: $required" >&2
    exit 1
  }
done

for required in \
  '--operation-policy PROFILE' \
  'strict-exclusive | exact-compatible-sharing' \
  'run --start requires explicit operation policy authority' \
  '--operation-policy is valid only for run --start' \
  'native_operation_policy::seal(*parsed.operation_policy)' \
  'std::optional<native_operation_policy> operation_policy' \
  'encode_native_operation_policy(*operation_policy)' \
  'decode_native_operation_policy(read_bytes(bytes, offset))' \
  'retained_evidence->operation_policy' \
  'admitted_operation_policy->snapshot()'; do
  grep -F -- "$required" "$root/cli/options.h" "$options" "$command" \
      >/dev/null || {
    echo "command admission omits native operation policy contract: $required" >&2
    exit 1
  }
done

# The selected policy is transaction-wide controller authority. Operation
# specifications describe operation-local facts and must not carry another
# encoded or caller-reconstructed planner policy body.
for required in \
  'pkgplan::package_policy_snapshot policy_' \
  'configuration_.policy()'; do
  grep -F -- "$required" "$operation_header" "$root/src/run_operation.cpp" \
      >/dev/null || {
    echo "native operation configuration omits admitted policy: $required" >&2
    exit 1
  }
done
specification_block=$(sed -n \
  '/class native_transaction_operation_specification final/,/class native_transaction_operation_configuration final/p' \
  "$operation_header")
if printf '%s\n' "$specification_block" \
    | grep -F -- 'package_policy_snapshot' >/dev/null 2>&1; then
  echo 'operation-local specification retains duplicate planner policy authority' >&2
  exit 1
fi
for forbidden in \
  'encode_incoming_policy' \
  'decode_incoming_policy' \
  'encode_normalized_policy' \
  'decode_normalized_policy' \
  'encode_policy(' \
  'decode_policy('; do
  if grep -F -- "$forbidden" "$operation_codec" >/dev/null 2>&1; then
    echo "operation-session codec owns foreign planner vocabulary: $forbidden" >&2
    exit 1
  fi
done

# CLI parsing and command evidence may name the pkgctl-owned profile, but may
# not expose or serialize libpkgplan's shared-ownership enum as command truth.
for file in "$options" "$command" "$operation_codec"; do
  if grep -F -- 'pkgplan::shared_ownership_policy' "$file" >/dev/null 2>&1; then
    echo "foreign shared-ownership enum escaped native policy adapter: $file" >&2
    exit 1
  fi
done
if grep -R -F -- '--shared-ownership' "$root/cli" "$root/tests/integration" \
    >/dev/null 2>&1; then
  echo 'partial raw planner policy switch remains exposed by pkgctl' >&2
  exit 1
fi

for required in \
  'snapshot.identity().string() == expected_identity' \
  'strict.snapshot().identity() != sharing.snapshot().identity()' \
  'native_operation_policy_error_code::invalid_profile' \
  'native_operation_policy_error_code::corrupt_encoding'; do
  grep -F -- "$required" "$unit" >/dev/null || {
    echo "native operation policy test omits hostile contract: $required" >&2
    exit 1
  }
done


for required in \
  'policy_drift_refused' \
  'policy_drift_specifications.calls() == 0U' \
  'policy_drift_bodies.calls() == 0U'; do
  grep -F -- "$required" "$root/tests/unit/effect_test.cpp" >/dev/null || {
    echo "operation restart omits retained-policy drift refusal: $required" >&2
    exit 1
  }
done

for required in \
  "require_help_text '--operation-policy PROFILE'" \
  'run unexpectedly accepted omitted operation policy authority' \
  'run unexpectedly accepted unknown operation policy profile' \
  'build unexpectedly accepted target-operation policy authority' \
  'run resume unexpectedly accepted operation-policy redeclaration'; do
  grep -F -- "$required" "$readonly" >/dev/null || {
    echo "read-only CLI policy qualification omits: $required" >&2
    exit 1
  }
done

for required in \
  '--operation-policy strict-exclusive' \
  'policy-operation-redeclaration' \
  '--operation-policy exact-compatible-sharing'; do
  grep -F -- "$required" "$native_construction" >/dev/null || {
    echo "native restart policy witness omits: $required" >&2
    exit 1
  }
done
