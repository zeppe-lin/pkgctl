# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

project(
  'pkgctl',
  'cpp',
  version: '0.4.0',
  license: 'GPL-3.0-or-later',
  meson_version: '>=1.2.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=3',
    'b_ndebug=if-release',
  ],
)

want_static = get_option('link_mode') == 'static'
pkgctl_inc = include_directories('include')

libcrypto_dep = dependency('libcrypto', required: true, static: want_static)
libpkgsource_dep = dependency(
  'libpkgsource',
  version: '>=1.1.0',
  required: true,
  static: want_static,
)
libpkgsource_yaml_dep = dependency(
  'libpkgsource-yaml',
  version: '>=1.1.0',
  required: true,
  static: want_static,
)
libpkgcatalog_dep = dependency(
  'libpkgcatalog',
  version: '>=1.1.0',
  required: true,
  static: want_static,
)
libpkgcatalog_acquire_dep = dependency(
  'libpkgcatalog-acquire',
  version: '>=1.1.0',
  required: true,
  static: want_static,
)
libpkgstate_dep = dependency(
  'libpkgstate',
  version: '>=2.2.0',
  required: true,
  static: want_static,
)
libpkgresolve_dep = dependency(
  'libpkgresolve',
  version: '>=1.0.0',
  required: true,
  static: want_static,
)
libpkgimage_dep = dependency(
  'libpkgimage',
  version: '>=0.3.0',
  required: true,
  static: want_static,
)
libpkgexec_dep = dependency(
  'libpkgexec',
  version: '>=1.2.0',
  required: true,
  static: want_static,
)
libpkgapply_dep = dependency(
  'libpkgapply',
  version: '>=1.0.0',
  required: true,
  static: want_static,
)
libpkgapply_exec_dep = dependency(
  'libpkgapply-exec',
  version: '>=0.1.0',
  required: true,
  static: want_static,
)
libpkgstate_apply_dep = dependency(
  'libpkgstate-apply',
  version: '>=2.2.0',
  required: true,
  static: want_static,
)
libpkgtransaction_dep = dependency(
  'libpkgtransaction',
  version: '>=1.1.0',
  required: true,
  static: want_static,
)

pkgctl_authority_deps = [
  libcrypto_dep,
  libpkgsource_dep,
  libpkgsource_yaml_dep,
  libpkgcatalog_dep,
  libpkgcatalog_acquire_dep,
  libpkgstate_dep,
  libpkgstate_apply_dep,
  libpkgimage_dep,
  libpkgexec_dep,
  libpkgapply_dep,
  libpkgapply_exec_dep,
  libpkgresolve_dep,
  libpkgtransaction_dep,
]

subdir('src')
subdir('cli')
subdir('man')
subdir('tests')
