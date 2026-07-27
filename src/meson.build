# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

pkgctl_core = static_library(
  'pkgctl-core',
  [
    'controller.cpp',
    'core.cpp',
    'error.cpp',
    'identity.cpp',
    'report.cpp',
    'request.cpp',
    'session.cpp',
  ],
  include_directories: pkgctl_inc,
  dependencies: pkgctl_authority_deps,
  install: false,
)

pkgctl_core_dep = declare_dependency(
  link_with: pkgctl_core,
  include_directories: pkgctl_inc,
  dependencies: pkgctl_authority_deps,
)
