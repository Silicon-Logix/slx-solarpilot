// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#pragma once
#include <filesystem>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif
namespace slx::desktop {
/** Resolve assets next to the portable executable, or above a development
 * bin directory. Direct launch and shortcuts do not depend on Explorer's cwd.
 * An explicit --root remains authoritative and is validated before startup. */
inline std::filesystem::path application_root(const std::filesystem::path &requested = {}) {
  auto valid = [](const auto &path) {
    return std::filesystem::is_regular_file(path / "SLX_SolarPilot/config/scene_contract.json") &&
           std::filesystem::is_regular_file(path / "config/capabilities.json");
  };
  if (!requested.empty()) {
    auto p = std::filesystem::absolute(requested);
    if (valid(p))
      return p;
    throw std::runtime_error("La cartella --root non contiene gli asset SolarPilot");
  }
#ifdef _WIN32
  wchar_t executable[32768]{};
  const DWORD n = GetModuleFileNameW(nullptr, executable, 32768);
  if (n && n < 32768) {
    auto p = std::filesystem::path(executable).parent_path();
    for (int i = 0; i < 5; ++i) {
      if (valid(p))
        return p;
      if (p == p.parent_path())
        break;
      p = p.parent_path();
    }
  }
#endif
  auto cwd = std::filesystem::current_path();
  if (valid(cwd))
    return cwd;
  throw std::runtime_error(
      "Asset SolarPilot assenti. Estrarre l'intera cartella del programma oppure indicare --root.");
}
} // namespace slx::desktop
