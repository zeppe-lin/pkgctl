#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
options="$srcdir/cli/options.cpp"
command="$srcdir/cli/run_command.cpp"
main="$srcdir/cli/main.cpp"
meson="$srcdir/cli/meson.build"
tests_meson="$srcdir/tests/meson.build"
integration="$srcdir/tests/integration/cli_run_test.sh"

for file in "$options" "$command" "$main" "$meson" "$tests_meson" "$integration"; do
  [ -s "$file" ] || {
    echo "missing bounded transaction command source: $file" >&2
    exit 1
  }
done

for required in \
  'enum class transaction_run_command_intent' \
  '--start SHA256' \
  '--resume SHA256' \
  '--max-steps N' \
  'class command_universe_store final' \
  'class private_effect_body_store final' \
  'class live_operation_authority final' \
  'public transaction_operation_session_sink' \
  'PKGCTL-OPERATION-OBSERVATIONS-1' \
  'retained.attempt_session()' \
  'record.stage() == effect_attempt_stage::application_intent' \
  'application_journals_.load_active(' \
  'pkgstate::posix::canonical_generation_store::open_existing(' \
  'native_posix_transaction_run_runtime::from_directory_fds(' \
  'require_native_execution_preflight(' \
  'transaction_run_drive_policy::make(command.maximum_steps)' \
  'runtime_path(command, "command-evidence")' \
  'runtime_path(command, "effect-bodies")' \
  'exact transaction run is already admitted; use --resume' \
  'exact transaction run is not admitted; use --start' \
  'retained command universe recomposes another transaction'; do
  grep -F -- "$required" "$srcdir/cli/options.h" "$options" "$command" \
      >/dev/null || {
    echo "missing bounded transaction command contract: $required" >&2
    exit 1
  }
done

for forbidden in \
  'create_director' \
  'directory_iterator' \
  'recursive_directory_iterator' \
  'canonical_generation_store::initialize' \
  'sleep(' \
  'usleep(' \
  'std::thread' \
  'std::async' \
  'getenv(' \
  'application_journals_.load(' \
  'latest_' \
  'list_'; do
  if grep -F "$forbidden" "$command" >/dev/null 2>&1; then
    echo "forbidden bounded transaction command policy: $forbidden" >&2
    exit 1
  fi
done

if grep -n -E 'while[[:space:]]*\([^)]*(complete|failed|quiescent|steps)' \
    "$command" >/dev/null 2>&1; then
  echo 'bounded command contains an implicit transaction drive loop' >&2
  exit 1
fi

grep -F "['main.cpp', 'options.cpp', 'run_command.cpp']" "$meson" >/dev/null || {
  echo 'bounded run command is not linked into the CLI' >&2
  exit 1
}
grep -F 'execute_transaction_run(std::move(request))' "$main" >/dev/null || {
  echo 'CLI does not dispatch the bounded transaction command' >&2
  exit 1
}
grep -F "'cli-run'" "$tests_meson" >/dev/null || {
  echo 'bounded run command has no process-level integration test' >&2
  exit 1
}
grep -F "suite: 'integration-privileged'" "$tests_meson" >/dev/null || {
  echo 'native mutating CLI test is not isolated as privileged integration' >&2
  exit 1
}
grep -F 'depends: native_interpreter' "$tests_meson" >/dev/null || {
  echo 'privileged CLI integration does not build its native interpreter fixture' >&2
  exit 1
}
grep -F 'build_by_default: true' "$tests_meson" >/dev/null || {
  echo 'native interpreter fixture is absent from the default test build graph' >&2
  exit 1
}
grep -F 'PKGCTL_REQUIRE_NATIVE_INTEGRATION' "$integration" >/dev/null || {
  echo 'privileged CLI integration has no release-required mode' >&2
  exit 1
}
for authority in \
  '--build-root' \
  '--lifecycle-root' \
  '--build-user-id' \
  '--build-group-id' \
  '--build-supplementary-group' \
  '--lifecycle-user-id' \
  '--lifecycle-group-id' \
  '--lifecycle-supplementary-group'; do
  grep -F -- "$authority" "$options" "$integration" >/dev/null || {
    echo "missing split run authority: $authority" >&2
    exit 1
  }
done
for obsolete in '--user-id' '--group-id' '--supplementary-group'; do
  if grep -F -- "$obsolete" "$options" "$srcdir/man/pkgctl.1.scd" \
      "$integration" >/dev/null 2>&1; then
    echo "obsolete shared execution authority remains: $obsolete" >&2
    exit 1
  fi
done
grep -F 'construction/check credentials must match the native supervisor' \
    "$command" "$integration" >/dev/null || {
  echo 'construction/check supervisor-credential refusal is not qualified' >&2
  exit 1
}
grep -F 'lifecycle credentials must match the native supervisor' \
    "$command" >/dev/null || {
  echo 'lifecycle supervisor-credential refusal missing' >&2
  exit 1
}
preflight_line=$(grep -n -F 'require_native_execution_preflight(' \
  "$command" | head -n 1 | cut -d: -f1)
retain_line=$(grep -n -F 'universes.retain(command.nonce, transaction)' \
  "$command" | head -n 1 | cut -d: -f1)
[ -n "$preflight_line" ] && [ -n "$retain_line" ] && \
    [ "$preflight_line" -lt "$retain_line" ] || {
  echo 'native execution preflight does not precede command-universe retention' >&2
  exit 1
}
for required_test in \
  'disposition step-limit-reached' \
  'durable-steps 1' \
  'exact transaction run is already admitted; use --resume' \
  'native execution unavailable before transaction execution;' \
  'unsupported native execution retained transaction evidence before refusal' \
  'rm -rf "$collection"' \
  'origin resumed' \
  'disposition completed' \
  'package fixture 1.0-1' \
  'durable-steps 0'; do
  grep -F "$required_test" "$integration" >/dev/null || {
    echo "missing process-level run qualification: $required_test" >&2
    exit 1
  }
done


for documented in \
  'Release 0.35.0 closes the functional package-management chain' \
  'Release 0.35.0 bounded native command boundary' \
  'Release 0.35.0 bounded native command qualification' \
  'Version 0.35.0 retains the native catalog' \
  'BOUNDED NATIVE TRANSACTION COMMAND'; do
  grep -F "$documented" "$srcdir/README.md" "$srcdir/DESIGN.md" \
      "$srcdir/TESTING.md" "$srcdir/man/pkgctl.1.scd" \
      "$srcdir/man/pkgctl_orchestration.7.scd" >/dev/null || {
    echo "missing bounded transaction command documentation: $documented" >&2
    exit 1
  }
done
