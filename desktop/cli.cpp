// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <slx/client.hpp>
#include <slx/version.hpp>
#include <thread>
extern "C" void SLX_SP_CALL slx_demo_policy(const slx_sp_policy_input *, slx_sp_policy_output *,
                                            void *);
namespace {
volatile std::sig_atomic_t stop_service = 0;
void stop_handler(int) {
  stop_service = 1;
}
} // namespace
/** Scriptable runner using exactly the same binary and protocol as the GUI.
 * --json-lines accepts one public API request per line; useful for experiments. */
int main(int argc, char **argv) {
  try {
    slx::Client core;
    core.policy(slx_demo_policy);
    int scenario = 0, seconds = 3600;
    std::string csv, record, policy = "balanced", listen_endpoint, controller_endpoint;
    int serve_seconds = 0;
    bool lines = false, eebus_ems = false;
    std::string peer_file, profile_file, state_dir = "workspace/eebus";
    for (int i = 1; i < argc; ++i) {
      const std::string a = argv[i];
      if (a == "--version") {
        std::cout << "SLX SolarPilot " SLX_SOLARPILOT_VERSION "\n";
        return 0;
      } else if (a == "--scenario" && i + 1 < argc)
        scenario = std::stoi(argv[++i]);
      else if (a == "--seconds" && i + 1 < argc)
        seconds = std::stoi(argv[++i]);
      else if (a == "--csv" && i + 1 < argc)
        csv = argv[++i];
      else if (a == "--record" && i + 1 < argc)
        record = argv[++i];
      else if (a == "--json-lines")
        lines = true;
      else if (a == "--eebus-ems")
        eebus_ems = true;
      else if (a == "--peer" && i + 1 < argc)
        peer_file = argv[++i];
      else if (a == "--profile" && i + 1 < argc)
        profile_file = argv[++i];
      else if (a == "--state-dir" && i + 1 < argc)
        state_dir = argv[++i];
      else if (a == "--policy" && i + 1 < argc)
        policy = argv[++i];
      else if (a == "--ems-listen" && i + 1 < argc)
        listen_endpoint = argv[++i];
      else if (a == "--controller" && i + 1 < argc)
        controller_endpoint = argv[++i];
      else if (a == "--serve-seconds" && i + 1 < argc)
        serve_seconds = std::stoi(argv[++i]);
      else {
        std::cout << "slx-solarpilot-cli [--scenario 0..6] [--seconds N] [--csv path] [--record "
                     "path] [--policy balanced|custom|manual] [--json-lines] [--controller "
                     "tcp://host:port]\n"
                     "slx-solarpilot-cli --ems-listen tcp://127.0.0.1:7447 [--serve-seconds N]\n";
        std::cout
            << "slx-solarpilot-cli --eebus-ems --peer workspace/eebus/plant-peer.json "
               "[--profile workspace/bench-profile.json] [--serve-seconds N]\n"
               "NNG flags above are for the accelerated laboratory only. The GUI uses EEBUS.\n";
        return a == "--help" ? 0 : 1;
      }
    }
    if (eebus_ems) {
      if (!profile_file.empty()) {
        std::ifstream f(profile_file);
        const auto p = slx::json::parse(f);
        core.call({{"op", "configure"}, {"patch", p.contains("config") ? p.at("config") : p}});
        if (p.contains("clock_hour"))
          core.call({{"op", "action"}, {"device", "clock"}, {"hour", p.at("clock_hour")}});
      }
      const auto ready = core.call({{"op", "eebus_start"},
                                    {"role", "controller"},
                                    {"state_dir", state_dir},
                                    {"port", 7448}});
      if (!peer_file.empty()) {
        std::ifstream f(peer_file);
        core.call({{"op", "eebus_pair"}, {"peer", slx::json::parse(f)}});
      }
      std::signal(SIGINT, stop_handler);
      std::signal(SIGTERM, stop_handler);
      std::cout << ready.at("link").at("identity").dump() << std::endl;
      const auto start = std::chrono::steady_clock::now();
      while (!stop_service && (serve_seconds <= 0 || std::chrono::steady_clock::now() - start <
                                                         std::chrono::seconds(serve_seconds))) {
        try {
          const double t =
              std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
          core.call({{"op", "eebus_tick"}, {"sim_s", t}});
        } catch (const std::exception &e) {
          std::cerr << e.what() << '\n';
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
      }
      return 0;
    }
    if (!listen_endpoint.empty()) {
      const auto ready = core.call({{"op", "listen_controller"}, {"endpoint", listen_endpoint}});
      std::signal(SIGINT, stop_handler);
      std::signal(SIGTERM, stop_handler);
      std::cout << ready.dump() << std::endl;
      const auto start = std::chrono::steady_clock::now();
      while (!stop_service && (serve_seconds <= 0 || std::chrono::steady_clock::now() - start <
                                                         std::chrono::seconds(serve_seconds)))
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      return 0;
    }
    if (!controller_endpoint.empty())
      core.call({{"op", "connect_controller"}, {"endpoint", controller_endpoint}});
    if (lines) {
      std::string line;
      while (std::getline(std::cin, line)) {
        try {
          std::cout << core.call(slx::json::parse(line)).dump() << '\n';
        } catch (const std::exception &e) {
          std::cout << slx::json({{"error", e.what()}}).dump() << '\n';
        }
      }
      return 0;
    }
    if (seconds < 1 || seconds > 172800)
      throw std::runtime_error("seconds outside 1..172800");
    auto state = core.call({{"op", "reset"}, {"scenario", scenario}});
    if (policy != "balanced" && policy != "custom" && policy != "manual")
      throw std::runtime_error("policy must be balanced, custom or manual");
    state = core.call({{"op", "configure"},
                       {"patch",
                        {{"auto", policy != "manual"},
                         {"policy", policy == "custom" ? "custom" : "balanced"},
                         {"manual_ev_w", policy == "manual" ? 3680.0 : 0.0}}}});
    std::ofstream out, log;
    if (!csv.empty()) {
      out.open(csv);
      if (!out)
        throw std::runtime_error("Cannot create CSV");
      out << "sim_s,pv_w,house_w,battery_w,ev_w,grid_w,soc,ev_soc,balance_w\n";
    }
    if (!record.empty()) {
      log.open(record);
      if (!log)
        throw std::runtime_error("Cannot create recording");
      log << slx::json({{"schema", "slx.solarpilot.recording/1"},
                        {"version", SLX_SOLARPILOT_VERSION},
                        {"initial", state}})
                 .dump()
          << '\n';
    }
    for (int i = 0; i < seconds; ++i) {
      state = core.call({{"op", "advance"}});
      if (out.is_open())
        out << state["sim_s"] << ',' << state["pv_w"] << ',' << state["house_w"] << ','
            << state["battery_w"] << ',' << state["ev_w"] << ',' << state["grid_w"] << ','
            << state["soc"] << ',' << state["ev_soc"] << ',' << state["balance_w"] << '\n';
      if (log.is_open() && (i % 10 == 0 || i == seconds - 1))
        log << slx::json({{"state", state}}).dump() << '\n';
    }
    std::cout << state.dump(2) << '\n';
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "SLX SolarPilot: " << e.what() << '\n';
    return 1;
  }
}
