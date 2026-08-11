#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

srcdir=${1:-.}
test_source=$srcdir/tests/integration/package_pipeline_test.cpp

[ -s "$test_source" ] || {
  echo 'missing package-pipeline integration source' >&2
  exit 1
}

for authority in \
  'pkgctl::acquire_catalog' \
  'pkgctl::resolve_packages' \
  'pkgctl::compose_transaction' \
  'pkgctl::transaction_run::begin' \
  'pkgctl::reserve_next' \
  'pkgctl::native_transaction_dispatch_session_source' \
  'pkgctl::native_construction_driver' \
  'pkgctl::execute_construction' \
  'pkgctl::complete_construction_dispatch' \
  'pkgctl::native_transaction_check_driver' \
  'pkgctl::execute_transaction_check' \
  'pkgctl::complete_check_dispatch' \
  'pkgctl::prepare_operation' \
  'pkgapply::posix::application_posix_backend::from_directory_fds' \
  'pkgapply::posix::target_mutation_lease::acquire' \
  'pkgstate::apply_adapter::read_application_state' \
  'pkgctl::explicit_transaction_effect_archive_source::make' \
  'pkgctl::acquire_transaction_effect_archive' \
  'pkgctl::native_transaction_effect_driver' \
  'pkgctl::posix_effect_journal_store::open' \
  'pkgctl::execute_effectful_operation_durable' \
  'pkgctl::submit_operation_dispatch_result' \
  'pkgctl::observe_native_target_paths' \
  'pkgreconcile::apply_adapter::project_completed_application' \
  'pkgreconcile::apply_posix::publish_verified_projection' \
  'pkgreconcile::posix::inventory_generation_store::open_existing'; do
  grep -F "$authority" "$test_source" >/dev/null || {
    echo "package-pipeline test bypasses production authority: $authority" >&2
    exit 1
  }
done

grep -F 'public pkgexec::execution_backend' "$test_source" >/dev/null || {
  echo 'package-pipeline test does not isolate process execution behind backend injection' >&2
  exit 1
}
grep -F 'dependency.session().paths().build.package_output_root' \
  "$test_source" >/dev/null || {
  echo 'package-pipeline test does not prove predecessor package-tree reuse' >&2
  exit 1
}
grep -F 'pkgexec::resource_role::build_input_tree, "dep"' \
  "$test_source" >/dev/null || {
  echo 'package-pipeline backend does not consume the dependency input slot' >&2
  exit 1
}

grep -F 'pkgexec::resource_role::build_input_tree, "checked-package"' \
  "$test_source" >/dev/null || {
  echo 'package-pipeline backend does not consume the constructed package during check' >&2
  exit 1
}

grep -F 'pkgsource::requirement_scope::check()' "$test_source" >/dev/null || {
  echo 'package-pipeline request does not explicitly ask for check authority' >&2
  exit 1
}
grep -F 'pkgsource::package_reference("tool")' "$test_source" >/dev/null || {
  echo 'package-pipeline check goal is not bound to the tool package' >&2
  exit 1
}

grep -F 'tool.session().paths().build.artifact_path' \
  "$test_source" >/dev/null || {
  echo 'package-pipeline application does not reopen the exact constructed artifact' >&2
  exit 1
}
grep -F 'installed.find_package("tool")' "$test_source" >/dev/null || {
  echo 'package-pipeline test does not verify canonical installed package state' >&2
  exit 1
}
grep -F 'application.target_root / "usr/bin/tool"' \
  "$test_source" >/dev/null || {
  echo 'package-pipeline test does not verify disposable target bytes' >&2
  exit 1
}
grep -F 'run.progress().complete()' "$test_source" >/dev/null || {
  echo 'package-pipeline test does not prove terminal transaction progression' >&2
  exit 1
}

grep -F 'pkgresolve::installed_preference::prefer_catalog' "$test_source" >/dev/null || {
  echo 'package-pipeline test does not force a catalog upgrade over installed state' >&2
  exit 1
}
grep -F 'pkgplan::rejected_object_policy::stage' "$test_source" >/dev/null || {
  echo 'package-pipeline upgrade does not stage protected incoming evidence' >&2
  exit 1
}
grep -F 'pkgplan::retained_active_ownership_policy::do_not_claim_operated_package' \
  "$test_source" >/dev/null || {
  echo 'package-pipeline upgrade does not relinquish protected-path ownership' >&2
  exit 1
}
grep -F 'pkgtransaction::convergence_policy::converge_exact()' "$test_source" >/dev/null || {
  echo 'package-pipeline removal does not exercise exact convergence' >&2
  exit 1
}
grep -F 'suppressed_resolved() == 1U' "$test_source" >/dev/null || {
  echo 'package-pipeline reconciliation does not prove anti-resurrection' >&2
  exit 1
}
grep -F 'application.target_root / "etc/tool.conf"' "$test_source" >/dev/null || {
  echo 'package-pipeline does not verify protected local target bytes' >&2
  exit 1
}

if grep -R -F 'libpkgreconcile' "$srcdir/meson.build" "$srcdir/src" >/dev/null 2>&1; then
  echo 'reconciliation qualification leaked into pkgctl production dependencies' >&2
  exit 1
fi

for forbidden in 'pkgctl_exe' 'std::system(' '::execv(' '::execve('; do
  if grep -F "$forbidden" "$test_source" >/dev/null; then
    echo "package-pipeline test escaped into command/process orchestration: $forbidden" >&2
    exit 1
  fi
done
