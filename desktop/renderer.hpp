// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#pragma once
#include <filesystem>
#include <memory>
#include <slx/client.hpp>
namespace slx::desktop {
struct RendererImpl;
/** glTF renderer. Coordinates remain in the exported Y-up basis. Geometry and
 * textures are uploaded once; visibility is inherited without changing scale. */
class Renderer {
  std::unique_ptr<RendererImpl> p;

public:
  explicit Renderer(const std::filesystem::path &asset_root);
  ~Renderer();
  unsigned render(int width, int height, const json &state, bool cutaway, bool showcase,
                  double clip_s, const json &mechanism_pose = json::object());
  /** Upload a product image once; the renderer owns its OpenGL lifetime. */
  unsigned image_file(const std::filesystem::path &path);
  void orbit(float dx, float dy);
  void zoom(float amount);
  void focus(int preset);
  void exposure(float value);
  std::string pick(float x, float y) const;
  json diagnostics() const;
  void screenshot(const std::filesystem::path &path, int width, int height);
};
} // namespace slx::desktop
