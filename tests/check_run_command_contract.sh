#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
options="$srcdir/cli/options.cpp"
command="$srcdir/cli/run_command.cpp"
main="$srcdir/cli/main.cpp"
meson="$srcdir/cli/meson.build"

for file in "$options" "$command" "$main" "$meson"; do
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
  'application_journals_.load_active(' \
  'pkgstate::posix::canonical_generation_store::open_existing(' \
  'native_posix_transaction_run_runtime::from_directory_fds(' \
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
