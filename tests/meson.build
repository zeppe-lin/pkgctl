# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if not get_option('tests').disabled()
  check_test = executable(
    'check-test',
    'check_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('check', check_test)

  construction_test = executable(
    'construction-test',
    'construction_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('construction', construction_test)

  dispatch_test = executable(
    'dispatch-test',
    'dispatch_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('dispatch', dispatch_test)

  run_journal_test = executable(
    'run-journal-test',
    'run_journal_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('run-journal', run_journal_test)

  request_test = executable(
    'request-test',
    'request_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('request', request_test)

  session_test = executable(
    'session-test',
    'session_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('session', session_test)

  effect_journal_test = executable(
    'effect-journal-test',
    'effect_journal_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect-journal', effect_journal_test)

  effect_inspect_test = executable(
    'effect-inspect-test',
    'effect_inspect_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect-inspect', effect_inspect_test)

  effect_test = executable(
    'effect-test',
    'effect_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('effect', effect_test)

  report_test = executable(
    'report-test',
    'report_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('report', report_test)

  version_test = executable(
    'version-test',
    'version_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('version', version_test)

  state_fixture = executable(
    'state-fixture',
    'state_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  run_store_fixture = executable(
    'run-store-fixture',
    'run_store_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  effect_store_fixture = executable(
    'effect-store-fixture',
    'effect_store_fixture.cpp',
    dependencies: pkgctl_core_dep,
  )

  cli_test = find_program('cli_test.sh')
  test(
    'cli',
    cli_test,
    args: [
      pkgctl_exe,
      state_fixture,
      run_store_fixture,
      effect_store_fixture,
      meson.project_version(),
    ],
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
    'run_journal.h',
    'run_journal_codec.h',
    'run_inspect.h',
    'run_launch.h',
    'run_native.h',
    'run_reconcile.h',
    'run_commit.h',
    'run_execute.h',
    'run_restart.h',
    'run_store.h',
    'session.h',
    'version.h',
  ]
  foreach header : pkgctl_headers
    header_test = executable(
      'public-header-' + header.underscorify(),
      'public_header_test.cpp',
      cpp_args: ['-DPKGCTL_TEST_HEADER="pkgctl/' + header + '"'],
      dependencies: pkgctl_core_dep,
    )
    test('public-header-' + header, header_test)
  endforeach

  construction_contract = find_program('check_construction_contract.sh')
  test('construction-contract', construction_contract, args: [meson.project_source_root()])

  preparation_contract = find_program('check_preparation_contract.sh')
  test('preparation-contract', preparation_contract, args: [meson.project_source_root()])

  progression_contract = find_program('check_progression_contract.sh')
  test('progression-contract', progression_contract, args: [meson.project_source_root()])

  check_contract = find_program('check_check_contract.sh')
  test('check-contract', check_contract, args: [meson.project_source_root()])

  dispatch_contract = find_program('check_dispatch_contract.sh')
  test('dispatch-contract', dispatch_contract, args: [meson.project_source_root()])

  run_journal_contract = find_program('check_run_journal_contract.sh')
  test('run-journal-contract', run_journal_contract, args: [meson.project_source_root()])

  run_execute_contract = find_program('check_run_execute_contract.sh')
  test('run-execute-contract', run_execute_contract, args: [meson.project_source_root()])

  run_reconcile_contract = find_program('check_run_reconcile_contract.sh')
  test('run-reconcile-contract', run_reconcile_contract, args: [meson.project_source_root()])

  run_admit_contract = find_program('check_run_admit_contract.sh')
  test('run-admit-contract', run_admit_contract, args: [meson.project_source_root()])

  run_authority_contract = find_program('check_run_authority_contract.sh')
  test('run-authority-contract', run_authority_contract, args: [meson.project_source_root()])

  run_advance_contract = find_program('check_run_advance_contract.sh')
  test('run-advance-contract', run_advance_contract, args: [meson.project_source_root()])

  run_driver_source_contract = find_program(
    'check_run_driver_source_contract.sh',
  )
  test(
    'run-driver-source-contract',
    run_driver_source_contract,
    args: [meson.project_source_root()],
  )

  run_drive_contract = find_program('check_run_drive_contract.sh')
  test('run-drive-contract', run_drive_contract, args: [meson.project_source_root()])

  run_launch_contract = find_program('check_run_launch_contract.sh')
  test('run-launch-contract', run_launch_contract, args: [meson.project_source_root()])

  effect_inspect_contract = find_program('check_effect_inspect_contract.sh')
  test(
    'effect-inspect-contract',
    effect_inspect_contract,
    args: [meson.project_source_root()],
  )

  effect_inspect_cli_contract = find_program(
    'check_effect_inspect_cli_contract.sh',
  )
  test(
    'effect-inspect-cli-contract',
    effect_inspect_cli_contract,
    args: [meson.project_source_root()],
  )

  run_inspect_contract = find_program('check_run_inspect_contract.sh')
  test('run-inspect-contract', run_inspect_contract, args: [meson.project_source_root()])

  run_inspect_cli_contract = find_program('check_run_inspect_cli_contract.sh')
  test(
    'run-inspect-cli-contract',
    run_inspect_cli_contract,
    args: [meson.project_source_root()],
  )

  source_contract = find_program('check_source_contract.sh')
  test('source-contract', source_contract, args: [meson.project_source_root()])

  effect_contract = find_program('check_effect_contract.sh')
  test('effect-contract', effect_contract, args: [meson.project_source_root()])

  restart_contract = find_program('check_restart_contract.sh')
  test('restart-contract', restart_contract, args: [meson.project_source_root()])

  manual_contract = find_program('check_manual_contract.sh')
  test('manual-contract', manual_contract, args: [meson.project_source_root()])

  release_metadata = find_program('check_release_metadata.sh')
  test('release-metadata', release_metadata, args: [meson.project_source_root()])
endif
