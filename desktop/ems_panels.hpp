// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#pragma once
#include "host.hpp"
namespace slx::desktop {
/** Operator settings shared by the local EMS and the external-controller bench. */
void ems_settings_panel(Host &host, const json &state);
/** The PC remains the physical plant. The external mode never substitutes the
 * local EMS when the selected controller is absent. */
void controller_bench_panel(Host &host, const json &state, const std::filesystem::path &root);
} // namespace slx::desktop
