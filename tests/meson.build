# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if not get_option('tests').disabled()
  construction_test = executable(
    'construction-test',
    'construction_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('construction', construction_test)

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

  cli_test = find_program('cli_test.sh')
  test('cli', cli_test, args: [pkgctl_exe, state_fixture])

  pkgctl_headers = [
    'construction.h',
    'controller.h',
    'effect.h',
    'effect_journal.h',
    'effect_journal_codec.h',
    'effect_restart.h',
    'effect_store.h',
    'error.h',
    'identity.h',
    'pkgctl.h',
    'preparation.h',
    'report.h',
    'request.h',
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
