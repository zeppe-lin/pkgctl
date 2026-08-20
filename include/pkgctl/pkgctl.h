// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file pkgctl.h
 *  \brief Complete internal pkgctl controller API.
 */
#pragma once

#include <pkgctl/check.h>
#include <pkgctl/check_codec.h>
#include <pkgctl/construction.h>
#include <pkgctl/construction_codec.h>
#include <pkgctl/dispatch.h>
#include <pkgctl/controller.h>
#include <pkgctl/effect.h>
#include <pkgctl/effect_inspect.h>
#include <pkgctl/effect_journal.h>
#include <pkgctl/effect_journal_codec.h>
#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>
#include <pkgctl/error.h>
#include <pkgctl/identity.h>
#include <pkgctl/native_policy.h>
#include <pkgctl/operation_codec.h>
#include <pkgctl/preparation.h>
#include <pkgctl/progression.h>
#include <pkgctl/report.h>
#include <pkgctl/request.h>
#include <pkgctl/run_journal.h>
#include <pkgctl/run_journal_codec.h>
#include <pkgctl/run_inspect.h>
#include <pkgctl/run_launch.h>
#include <pkgctl/run_locator.h>
#include <pkgctl/run_native.h>
#include <pkgctl/run_nonce.h>
#include <pkgctl/run_operation.h>
#include <pkgctl/run_reconcile.h>
#include <pkgctl/run_recovery.h>
#include <pkgctl/run_progress.h>
#include <pkgctl/run_resource.h>
#include <pkgctl/run_authority.h>
#include <pkgctl/run_admit.h>
#include <pkgctl/run_advance.h>
#include <pkgctl/run_commit.h>
#include <pkgctl/run_cleanup.h>
#include <pkgctl/run_drive.h>
#include <pkgctl/run_evidence.h>
#include <pkgctl/run_evidence_codec.h>
#include <pkgctl/run_evidence_store.h>
#include <pkgctl/run_execute.h>
#include <pkgctl/run_restart.h>
#include <pkgctl/run_runtime.h>
#include <pkgctl/run_store.h>
#include <pkgctl/session.h>
#include <pkgctl/target_observation.h>
#include <pkgctl/version.h>
