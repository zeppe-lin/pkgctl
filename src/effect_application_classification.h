// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <libpkgapply/result.h>

#include <stdexcept>

namespace pkgctl::detail {

enum class application_effect_classification {
  completed,
  definitive_failure,
  external_resolution_required,
};

[[nodiscard]] inline application_effect_classification
classify_application_effect(pkgapply::application_attempt_outcome outcome)
{
  switch (outcome)
  {
    case pkgapply::application_attempt_outcome::completed:
      return application_effect_classification::completed;
    case pkgapply::application_attempt_outcome::precondition_refused:
    case pkgapply::application_attempt_outcome::failed_before_target_mutation:
    case pkgapply::application_attempt_outcome::failed_fully_recovered:
      return application_effect_classification::definitive_failure;
    case pkgapply::application_attempt_outcome::failed_with_partial_effects:
    case pkgapply::application_attempt_outcome::
        effects_visible_durability_unconfirmed:
    case pkgapply::application_attempt_outcome::indeterminate:
      return application_effect_classification::external_resolution_required;
  }
  throw std::invalid_argument("invalid application attempt outcome");
}

} // namespace pkgctl::detail
