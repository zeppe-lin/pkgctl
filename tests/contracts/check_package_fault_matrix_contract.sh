#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
test_source=$srcdir/tests/integration/package_pipeline_test.cpp
test_meson=$srcdir/tests/meson.build

[ -s "$test_source" ] || {
  echo 'missing package-pipeline integration source' >&2
  exit 1
}

for evidence in \
  'pipeline_execution_fault' \
  'pipeline_execution_fault::dependency_build' \
  'pipeline_execution_fault::package_check' \
  'pkgctl::execute_operation_dispatch_durable' \
  'pkgctl::transaction_run_restart_checkpoint::make' \
  'pkgctl::reconcile_operation_dispatch_durable' \
  'pkgapply::posix::application_journal_store::open' \
  '.load_active(' \
  'pkgctl::effect_attempt_stage::publication_terminal' \
  'pkgctl::effect_attempt_stage::application_terminal' \
  'application_resume_calls()' \
  'publication_calls() == 0U' \
  'reopen_run_head(' \
  'effect_store = pkgctl::posix_effect_journal_store::open(' \
  'reacquire_application_lease(' \
  'check_pipeline_build_failure' \
  'check_pipeline_check_failure' \
  'run.progress().failed()' \
  'transaction_node_status::blocked'; do
  grep -F "$evidence" "$test_source" >/dev/null || {
    echo "package fault/restart matrix lacks evidence: $evidence" >&2
    exit 1
  }
done

grep -F "'package-failure-matrix'" "$test_meson" >/dev/null || {
  echo 'package failure matrix is not registered as an integration test' >&2
  exit 1
}
grep -F "args: ['--failure-matrix']" "$test_meson" >/dev/null || {
  echo 'package failure matrix does not execute the shared vertical harness mode' >&2
  exit 1
}

grep -F '"$tmp/package-pipeline-test" --failure-matrix' \
  "$srcdir/tests/run-direct.sh" >/dev/null || {
  echo 'direct compiler harness does not execute the package failure matrix' >&2
  exit 1
}

for forbidden in \
  'pkgctl_exe' \
  'std::system(' \
  '::popen(' \
  '::fork(' \
  '::execv(' \
  '::execve('; do
  if grep -F "$forbidden" "$test_source" >/dev/null; then
    echo "package fault/restart matrix escaped into command/process orchestration: $forbidden" >&2
    exit 1
  fi
done
