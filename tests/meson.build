# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if not get_option('tests').disabled()
  check_test = executable(
    'check-test',
    'unit/check_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('check', check_test, suite: 'unit')

  construction_test = executable(
    'construction-test',
    'unit/construction_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('construction', construction_test, suite: 'unit')

  dispatch_test = executable(
    'dispatch-test',
    'unit/dispatch_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('dispatch', dispatch_test, suite: 'unit')

  run_journal_test = executable(
    'run-journal-test',
    'unit/run_journal_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('run-journal', run_journal_test, suite: 'unit')

  run_progress_test = executable(
    'run-progress-test',
    'unit/run_progress_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('run-progress', run_progress_test, suite: 'unit')

  run_locator_test = executable(
    'run-locator-test',
    'unit/run_locator_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('run-locator', run_locator_test, suite: 'unit')

  request_test = executable(
    'request-test',
    'unit/request_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('request', request_test, suite: 'unit')

  session_test = executable(
    'session-test',
    'unit/session_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('session', session_test, suite: 'unit')

  effect_journal_test = executable(
    'effect-journal-test',
    'unit/effect_journal_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect-journal', effect_journal_test, suite: 'unit')

  effect_inspect_test = executable(
    'effect-inspect-test',
    'unit/effect_inspect_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect-inspect', effect_inspect_test, suite: 'unit')

  effect_test = executable(
    'effect-test',
    'unit/effect_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect', effect_test, suite: 'unit')

  report_test = executable(
    'report-test',
    'unit/report_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('report', report_test, suite: 'unit')

  version_test = executable(
    'version-test',
    'unit/version_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('version', version_test, suite: 'unit')

  state_fixture = executable(
    'state-fixture',
    'fixtures/state_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  run_store_fixture = executable(
    'run-store-fixture',
    'fixtures/run_store_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  effect_store_fixture = executable(
    'effect-store-fixture',
    'fixtures/effect_store_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  cli_readonly_test = find_program('integration/cli_readonly_test.sh')
  test(
    'cli-readonly',
    cli_readonly_test,
    args: [
      pkgctl_exe,
      state_fixture,
      run_store_fixture,
      effect_store_fixture,
      meson.project_version(),
    ],
    suite: 'integration',
  )

  pkgctl_headers = [
    'check.h',
    'construction.h',
    'dispatch.h',
    'controller.h',
    'effect.h',
    'effect_inspect.h',
    'effect_journal.h',
    'effect_journal_codec.h',
    'effect_restart.h',
    'effect_store.h',
    'error.h',
    'identity.h',
    'pkgctl.h',
    'preparation.h',
    'progression.h',
    'report.h',
    'request.h',
    'run_admit.h',
    'run_authority.h',
    'run_advance.h',
    'run_drive.h',
    'run_evidence.h',
    'run_evidence_codec.h',
    'run_evidence_store.h',
    'run_journal.h',
    'run_journal_codec.h',
    'run_inspect.h',
    'run_launch.h',
    'run_locator.h',
    'run_native.h',
    'run_nonce.h',
    'run_operation.h',
    'run_reconcile.h',
    'run_recovery.h',
    'run_progress.h',
    'run_commit.h',
    'run_execute.h',
    'run_restart.h',
    'run_runtime.h',
    'run_store.h',
    'session.h',
    'version.h',
  ]
  foreach header : pkgctl_headers
    header_test = executable(
      'public-header-' + header.underscorify(),
      'unit/public_header_test.cpp',
      cpp_args: ['-DPKGCTL_TEST_HEADER="pkgctl/' + header + '"'],
      dependencies: pkgctl_core_dep,
    )
    test('public-header-' + header, header_test, suite: 'header')
  endforeach

  construction_contract = find_program('contracts/check_construction_contract.sh')
  test('construction-contract', construction_contract, args: [meson.project_source_root()], suite: 'contract')

  preparation_contract = find_program('contracts/check_preparation_contract.sh')
  test('preparation-contract', preparation_contract, args: [meson.project_source_root()], suite: 'contract')

  progression_contract = find_program('contracts/check_progression_contract.sh')
  test('progression-contract', progression_contract, args: [meson.project_source_root()], suite: 'contract')

  check_contract = find_program('contracts/check_check_contract.sh')
  test('check-contract', check_contract, args: [meson.project_source_root()], suite: 'contract')

  dispatch_contract = find_program('contracts/check_dispatch_contract.sh')
  test('dispatch-contract', dispatch_contract, args: [meson.project_source_root()], suite: 'contract')

  run_journal_contract = find_program('contracts/check_run_journal_contract.sh')
  test('run-journal-contract', run_journal_contract, args: [meson.project_source_root()], suite: 'contract')

  run_execute_contract = find_program('contracts/check_run_execute_contract.sh')
  test('run-execute-contract', run_execute_contract, args: [meson.project_source_root()], suite: 'contract')

  run_evidence_contract = find_program('contracts/check_run_evidence_contract.sh')
  test(
    'run-evidence-contract',
    run_evidence_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_recovery_contract = find_program('contracts/check_run_recovery_contract.sh')
  test(
    'run-recovery-contract',
    run_recovery_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_progress_contract = find_program('contracts/check_run_progress_contract.sh')
  test(
    'run-progress-contract',
    run_progress_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_reconcile_contract = find_program('contracts/check_run_reconcile_contract.sh')
  test('run-reconcile-contract', run_reconcile_contract, args: [meson.project_source_root()], suite: 'contract')

  run_admit_contract = find_program('contracts/check_run_admit_contract.sh')
  test('run-admit-contract', run_admit_contract, args: [meson.project_source_root()], suite: 'contract')

  run_authority_contract = find_program('contracts/check_run_authority_contract.sh')
  test('run-authority-contract', run_authority_contract, args: [meson.project_source_root()], suite: 'contract')

  run_locator_contract = find_program('contracts/check_run_locator_contract.sh')
  test(
    'run-locator-contract',
    run_locator_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_advance_contract = find_program('contracts/check_run_advance_contract.sh')
  test('run-advance-contract', run_advance_contract, args: [meson.project_source_root()], suite: 'contract')

  run_driver_source_contract = find_program(
    'contracts/check_run_driver_source_contract.sh',
  )
  test(
    'run-driver-source-contract',
    run_driver_source_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_operation_contract = find_program('contracts/check_run_operation_contract.sh')
  test(
    'run-operation-contract',
    run_operation_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_native_contract = find_program('contracts/check_run_native_contract.sh')
  test(
    'run-native-contract',
    run_native_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_nonce_contract = find_program('contracts/check_run_nonce_contract.sh')
  test(
    'run-nonce-contract',
    run_nonce_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_runtime_contract = find_program('contracts/check_run_runtime_contract.sh')
  test(
    'run-runtime-contract',
    run_runtime_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_drive_contract = find_program('contracts/check_run_drive_contract.sh')
  test('run-drive-contract', run_drive_contract, args: [meson.project_source_root()], suite: 'contract')

  run_launch_contract = find_program('contracts/check_run_launch_contract.sh')
  test('run-launch-contract', run_launch_contract, args: [meson.project_source_root()], suite: 'contract')

  effect_inspect_contract = find_program('contracts/check_effect_inspect_contract.sh')
  test(
    'effect-inspect-contract',
    effect_inspect_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  effect_inspect_cli_contract = find_program(
    'contracts/check_effect_inspect_cli_contract.sh',
  )
  test(
    'effect-inspect-cli-contract',
    effect_inspect_cli_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_command_contract = find_program('contracts/check_run_command_contract.sh')
  test(
    'run-command-contract',
    run_command_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  run_inspect_contract = find_program('contracts/check_run_inspect_contract.sh')
  test('run-inspect-contract', run_inspect_contract, args: [meson.project_source_root()], suite: 'contract')

  run_inspect_cli_contract = find_program('contracts/check_run_inspect_cli_contract.sh')
  test(
    'run-inspect-cli-contract',
    run_inspect_cli_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  source_contract = find_program('contracts/check_source_contract.sh')
  test('source-contract', source_contract, args: [meson.project_source_root()], suite: 'contract')

  cli_yaml_boundary = find_program('contracts/check_cli_yaml_boundary_contract.sh')
  test(
    'cli-yaml-boundary-contract',
    cli_yaml_boundary,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  effect_contract = find_program('contracts/check_effect_contract.sh')
  test('effect-contract', effect_contract, args: [meson.project_source_root()], suite: 'contract')

  restart_contract = find_program('contracts/check_restart_contract.sh')
  test('restart-contract', restart_contract, args: [meson.project_source_root()], suite: 'contract')

  manual_contract = find_program('contracts/check_manual_contract.sh')
  test('manual-contract', manual_contract, args: [meson.project_source_root()], suite: 'contract')

  test_layout_contract = find_program('contracts/check_test_layout_contract.sh')
  test(
    'test-layout-contract',
    test_layout_contract,
    args: [meson.project_source_root()],
    suite: 'contract',
  )

  release_metadata = find_program('contracts/check_release_metadata.sh')
  test('release-metadata', release_metadata, args: [meson.project_source_root()], suite: 'contract')
endif
