// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file target_observation.h
 *  \brief Native target observation admitted to planner authority.
 */
#pragma once

#include <vector>

#include <pkgctl/run_journal.h>

#include <libpkgapply-posix/target_observer.h>
#include <libpkgplan/observation.h>

namespace pkgctl {

/*! \brief Observe one exact live target path set for a reserved operation.
 *
 * The caller supplies the durable run record, its exact current progress and
 * the reserved operation dispatch.  The function validates that these values
 * name one authority epoch, performs descriptor-anchored POSIX observation,
 * translates complete application facts into planner facts, and seals the
 * resulting observation set in the controller's stable identity domain.
 *
 * This function performs observation only.  Retention/replay of the returned
 * evidence remains orchestration policy owned by the caller.
 */
[[nodiscard]] pkgplan::target_observation_set observe_native_target_paths(
    const transaction_run_journal_record& record,
    const transaction_progress& progress,
    const transaction_dispatch& dispatch,
    const pkgplan::target_system_context_identity& target,
    pkgapply::posix::application_target_observer& observer,
    std::vector<pkgplan::package_path> requested,
    std::vector<pkgapply::posix::target_hardlink_expectation> hardlinks = {});

} // namespace pkgctl
