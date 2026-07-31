// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file pkgctl.h
 *  \brief Complete internal pkgctl controller API.
 */
#pragma once

#include <pkgctl/check.h>
#include <pkgctl/construction.h>
#include <pkgctl/dispatch.h>
#include <pkgctl/controller.h>
#include <pkgctl/effect.h>
#include <pkgctl/effect_journal.h>
#include <pkgctl/effect_journal_codec.h>
#include <pkgctl/effect_restart.h>
#include <pkgctl/effect_store.h>
#include <pkgctl/error.h>
#include <pkgctl/identity.h>
#include <pkgctl/preparation.h>
#include <pkgctl/progression.h>
#include <pkgctl/report.h>
#include <pkgctl/request.h>
#include <pkgctl/run_journal.h>
#include <pkgctl/run_journal_codec.h>
#include <pkgctl/run_commit.h>
#include <pkgctl/run_execute.h>
#include <pkgctl/run_restart.h>
#include <pkgctl/run_store.h>
#include <pkgctl/session.h>
#include <pkgctl/version.h>
