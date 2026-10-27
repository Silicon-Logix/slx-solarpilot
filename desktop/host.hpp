// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#pragma once
#include <atomic>
#include <deque>
#include <filesystem>
#include <mutex>
#include <slx/client.hpp>
#include <thread>
namespace slx::desktop {
struct Sample {
  double t, pv, house, battery, ev, grid, soc;
};
/** Read a bounded, versioned recording. Reject malformed snapshots before the
 * GUI sees them; playback never sends saved requests back to the controller. */
std::vector<json> read_recording(const std::filesystem::path &path);
/** Independent experiments through the public API, preserving the live session. */
json compare_policies(int scenario, int seconds = 7200);
/** Owns the core on one worker thread. OpenGL never waits for network I/O.
 * Rendering uses confirmed snapshots; bounded queues prevent unbounded growth. */
class Host {
  std::mutex mutex_;
  std::thread worker_;
  std::atomic<bool> stop_{false};
  std::deque<json> commands_;
  json state_, packets_ = json::array();
  std::vector<Sample> history_;
  std::string error_, recording_;
  bool running_ = false;
  double speed_ = 60;
  bool eebus_;
  void run();

public:
  explicit Host(bool eebus = true);
  ~Host();
  void submit(json request);
  json state();
  json packets();
  std::vector<Sample> history();
  std::string error();
  void playback(bool running, double speed);
  void export_csv(const std::filesystem::path &path);
};
} // namespace slx::desktop
