# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

pkgctl_core = static_library(
  'pkgctl-core',
  [
    'core.cpp',
    'constraint.cpp',
    'error.cpp',
    'intent.cpp',
    'outcome.cpp',
    'package.cpp',
  ],
  include_directories: pkgctl_inc,
  install: false,
)

pkgctl_core_dep = declare_dependency(
  link_with: pkgctl_core,
  include_directories: pkgctl_inc,
)
