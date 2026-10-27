// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#pragma once
#include <filesystem>
#include <slx/client.hpp>
namespace slx::desktop {
/** Local product information and bundled legal/model documents. No account or
 * network request is required to read these panels. */
/** Set the shared Silicon LogiX logo uploaded by the active renderer. */
void product_brand_image(unsigned texture);
void product_about_panel(const std::filesystem::path &root, const json &diagnostics);
/** Describe scoped engineering services and prepare a local request brief.
 * The contact page opens only after an explicit operator click; no data is sent. */
void product_customization_panel(const std::filesystem::path &root, const json &state);
/** Product-wide keyboard dialogs, following the other SLX desktop projects. */
void product_dialogs(const std::filesystem::path &root, const json &diagnostics, bool &about,
                     bool &guide);
} // namespace slx::desktop
