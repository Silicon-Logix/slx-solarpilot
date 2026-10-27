// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#pragma once
#include <filesystem>
#include <slx/client.hpp>
namespace slx::desktop {
/** Observe the live application for the separate PC Regia without drawing
 * on the recording surface or changing energy values. */
void presentation_overlay(const std::filesystem::path &root,const json &state,int page,bool replaying);
/** Publish an acknowledgement only after the frame was actually presented. */
void presentation_frame_presented();
}
