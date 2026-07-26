# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

if not get_option('tests').disabled()
  model_test = executable(
    'model-test',
    'model_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('model', model_test)

  version_test = executable(
    'version-test',
    'version_test.cpp',
    dependencies: pkgctl_core_dep,
  )
  test('version', version_test)

  cli_test = find_program('cli_test.sh')
  test('cli', cli_test, args: [pkgctl_exe])

  public_headers = find_program('public_headers.sh')
  test('public-headers', public_headers, args: [meson.project_source_root()])

  source_contract = find_program('check_source_contract.sh')
  test('source-contract', source_contract, args: [meson.project_source_root()])
endif
