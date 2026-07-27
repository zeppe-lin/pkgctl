# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if not get_option('tests').disabled()
  request_test = executable(
    'request-test',
    'request_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('request', request_test)

  version_test = executable(
    'version-test',
    'version_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('version', version_test)

  pkgctl_headers = [
    'error.h',
    'identity.h',
    'pkgctl.h',
    'request.h',
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

  source_contract = find_program('check_source_contract.sh')
  test('source-contract', source_contract, args: [meson.project_source_root()])

  manual_contract = find_program('check_manual_contract.sh')
  test('manual-contract', manual_contract, args: [meson.project_source_root()])

  release_metadata = find_program('check_release_metadata.sh')
  test('release-metadata', release_metadata, args: [meson.project_source_root()])
endif
