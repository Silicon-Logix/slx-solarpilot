// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include "../desktop/host.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using slx::json;
template <typename Predicate> void wait_until(Predicate predicate) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (!predicate()) {
    if (std::chrono::steady_clock::now() > deadline)
      throw std::runtime_error("Host deadline exceeded");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}
int main() {
  try {
    const auto file = std::filesystem::current_path() / "solarpilot-host-test.slxrun";
    {
      slx::desktop::Host host(false);
      wait_until([&] { return !host.state().is_null(); });
      host.submit({{"op", "record_start"}, {"path", file.string()}});
      host.submit({{"op", "advance"}, {"steps", 60}});
      wait_until([&] {
        const auto s = host.state();
        return s.at("sim_s") == 60 && s.value("recording", false);
      });
      host.submit({{"op", "record_stop"}});
      wait_until([&] { return !host.state().value("recording", true); });
      host.submit({{"op", "action"}, {"device", "washer"}, {"on", false}});
      wait_until([&] { return !host.error().empty(); });
      if (host.history().empty() || host.packets().empty())
        throw std::runtime_error("Host did not publish history/packets");
    }
    std::ifstream stream(file);
    std::string line;
    std::getline(stream, line);
    if (json::parse(line).at("schema") != "slx.solarpilot.recording/1")
      throw std::runtime_error("Recording header invalid");
    int snapshots = 0;
    while (std::getline(stream, line)) {
      const auto row = json::parse(line);
      if (row.contains("state")) {
        ++snapshots;
        if (row["state"]["sim_s"] != 60)
          throw std::runtime_error("Unexpected recorded state");
      }
    }
    if (snapshots < 1)
      throw std::runtime_error("No confirmed snapshots recorded");
    stream.close();
    const auto replay = slx::desktop::read_recording(file);
    if (replay.size() < 2 || replay.back().at("sim_s") != 60)
      throw std::runtime_error("Replay differs from confirmed recording");
    {
      std::ofstream malformed(file);
      malformed << "{\"schema\":\"slx.solarpilot.recording/1\",\"initial\":{}}\n";
    }
    bool rejected = false;
    try {
      slx::desktop::read_recording(file);
    } catch (...) {
      rejected = true;
    }
    if (!rejected)
      throw std::runtime_error("Malformed recording accepted");
    std::filesystem::remove(file);
    const auto comparison = slx::desktop::compare_policies(0, 120);
    if (comparison["results"].size() != 3)
      throw std::runtime_error("Policy comparison incomplete");
    for (const auto &r : comparison["results"])
      if (r["sim_s"] != 120 || r["balance_w"].get<double>() > 1e-6)
        throw std::runtime_error("Policy comparison interval or balance invalid");
    std::cout << "Async host, confirmed snapshots, recording stop, public errors: OK\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
