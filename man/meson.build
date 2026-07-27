# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later

scdoc = find_program('scdoc', required: get_option('man_pages'))
if scdoc.found()
  foreach page : ['pkgctl.1', 'pkgctl_orchestration.7']
    custom_target(
      page,
      input: page + '.scd',
      output: page,
      command: [scdoc],
      feed: true,
      capture: true,
      install: true,
      install_dir: get_option('mandir') / ('man' + page.split('.')[1]),
    )
  endforeach
endif
