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
  'upgrade_bodies.application()' \
  'removal_bodies.application()' \
  'application_resume_calls() == 0U' \
  'publication_calls() == 0U' \
  'reopen_run_head(' \
  'effect_store = pkgctl::posix_effect_journal_store::open(' \
  'reacquire_application_lease(' \
  'read_historical_application_state(' \
  'publication_state_projection() const noexcept override' \
  'return delegate_.publication_state_projection();' \
  'check_pipeline_build_failure' \
  'check_pipeline_check_failure' \
  'check_native_runtime_pre_operation_failure' \
  'pkgctl::native_posix_transaction_run_runtime::open' \
  'pkgctl::transaction_run_drive_disposition::stopped_after_failure' \
  'backend.build_calls() == expected_build_calls' \
  'backend.check_calls() == expected_check_calls' \
  'operations.operation_calls() == 0U' \
  'operations.archive_calls() == 0U' \
  'reopened.record().identity() == failed_record' \
  'pkgctl::transaction_run_advance_disposition::quiescent' \
  'run.progress().failed()' \
  'transaction_node_status::blocked' \
  'runtime_operation_failure' \
  'runtime_operation_failure::application' \
  'runtime_operation_failure::pre_install_lifecycle' \
  'runtime_operation_failure::post_install_lifecycle' \
  'runtime_operation_failure::publication' \
  'pipeline_lifecycle_resolution_request' \
  'pkgsource::requirement_scope::lifecycle(action)' \
  'faulting_application_backend' \
  'pkgapply::backend_operation_outcome::failed' \
  'failing_canonical_store' \
  'failed_before_publication()' \
  'pkgctl::effectful_operation_outcome::application_not_completed' \
  'lifecycle_failed_before_application' \
  'lifecycle_failed_after_application' \
  'state_publication_not_completed' \
  'effect_bodies.lifecycle_count() == 1U' \
  'effect_bodies.lifecycle_count() == 2U' \
  'fs::create_directories(lifecycle_sessions)' \
  'fs::is_directory(entry.path() / "tmp/home")' \
  'lifecycle_session_count() == expected_lifecycle_sessions' \
  'application_failure.active_calls() == application_active_calls' \
  'publication_failure.publication_calls() == publication_calls' \
  'operations.archive_calls() == archive_calls' \
  'check_native_runtime_operation_failure' \
  'runtime_publication_intent_resolution' \
  'runtime_publication_intent_resolution::resulting_visible' \
  'runtime_publication_intent_resolution::retry_from_prior' \
  'published_canonical_store' \
  'indeterminate_prior_canonical_store' \
  'publication_intent_interrupting_effect_driver' \
  'pkgctl::commit_transaction_run_successor' \
  'pkgctl::commit_operation_dispatch_start' \
  'state_publication_backend_result::indeterminate' \
  'pkgstate::state_publication_outcome::indeterminate' \
  'pkgctl::transaction_run_drive_disposition::step_limit_reached' \
  'pkgctl::transaction_run_advance_disposition::reconciled_operation' \
  'state_store.publication_calls() == 1U' \
  'effect_store.append(' \
  'pkgctl::effect_attempt_stage::before_lifecycle_intent' \
  'pkgctl::transaction_run_drive_disposition::external_resolution_required' \
  'unresolved.durable_step_count() == 0U' \
  'check_native_runtime_publication_intent_uncertainty' \
  'check_native_runtime_terminal_indeterminate_publication' \
  'check_native_runtime_lifecycle_intent_external_resolution'; do
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

grep -F "'package-operation-failure-matrix'" "$test_meson" >/dev/null || {
  echo 'package operation failure matrix is not registered as an integration test' >&2
  exit 1
}
grep -F "args: ['--operation-failure-matrix']" "$test_meson" >/dev/null || {
  echo 'package operation failure matrix does not execute the shared vertical harness mode' >&2
  exit 1
}

grep -F "'package-operation-uncertainty-matrix'" "$test_meson" >/dev/null || {
  echo 'package operation uncertainty matrix is not registered as an integration test' >&2
  exit 1
}
grep -F "args: ['--operation-uncertainty-matrix']" "$test_meson" >/dev/null || {
  echo 'package operation uncertainty matrix does not execute the shared vertical harness mode' >&2
  exit 1
}

grep -F '"$tmp/package-pipeline-test" --failure-matrix' \
  "$srcdir/tests/run-direct.sh" >/dev/null || {
  echo 'direct compiler harness does not execute the package failure matrix' >&2
  exit 1
}

grep -F '"$tmp/package-pipeline-test" --operation-failure-matrix' \
  "$srcdir/tests/run-direct.sh" >/dev/null || {
  echo 'direct compiler harness does not execute the package operation failure matrix' >&2
  exit 1
}

grep -F '"$tmp/package-pipeline-test" --operation-uncertainty-matrix' \
  "$srcdir/tests/run-direct.sh" >/dev/null || {
  echo 'direct compiler harness does not execute the package operation uncertainty matrix' >&2
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
