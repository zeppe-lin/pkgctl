# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

pkgctl_core = static_library(
  'pkgctl-core',
  [
    'check.cpp',
    'construction.cpp',
    'dispatch.cpp',
    'controller.cpp',
    'core.cpp',
    'effect.cpp',
    'effect_journal.cpp',
    'effect_journal_codec.cpp',
    'effect_restart.cpp',
    'effect_store.cpp',
    'error.cpp',
    'identity.cpp',
    'preparation.cpp',
    'progression.cpp',
    'report.cpp',
    'run_journal.cpp',
    'run_journal_codec.cpp',
    'run_commit.cpp',
    'run_execute.cpp',
    'run_restart.cpp',
    'run_store.cpp',
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
