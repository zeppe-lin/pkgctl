# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

pkgctl_exe = executable(
  'pkgctl',
  ['main.cpp', 'options.cpp', 'execution_report.cpp', 'run_command.cpp'],
  dependencies: [
    pkgctl_core_dep,
    libpkgsource_yaml_dep,
    libpkgcatalog_codec_dep,
    libpkgexec_linux_dep,
  ],
  install: true,
  install_dir: get_option('bindir'),
)
