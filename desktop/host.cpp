// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include "host.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#endif
#include <slx/version.hpp>
extern "C" void SLX_SP_CALL slx_demo_policy(const slx_sp_policy_input *, slx_sp_policy_output *,
                                            void *);
namespace slx::desktop {
json compare_policies(int scenario, int seconds) {
  if (seconds < 1 || seconds > 86400)
    throw std::runtime_error("Durata confronto fuori intervallo");
  json result = {{"schema", "slx.solarpilot.policy-comparison/1"},
                 {"scenario", scenario},
                 {"duration_s", seconds},
                 {"results", json::array()}};
  for (const char *mode : {"manual", "balanced", "custom"}) {
    Client core;
    core.policy(slx_demo_policy);
    core.call({{"op", "reset"}, {"scenario", scenario}});
    core.call({{"op", "configure"},
               {"patch",
                {{"auto", std::string(mode) != "manual"},
                 {"policy", std::string(mode) == "custom" ? "custom" : "balanced"},
                 {"manual_ev_w", 3680.0},
                 {"manual_battery_w", 0.0}}}});
    json state;
    for (int left = seconds; left > 0;) {
      const int count = std::min(left, 3600);
      state = core.call({{"op", "advance"}, {"steps", count}});
      left -= count;
    }
    result["results"].push_back({{"policy", mode},
                                 {"energy", state.at("energy")},
                                 {"soc", state.at("soc")},
                                 {"ev_soc", state.at("ev_soc")},
                                 {"sim_s", state.at("sim_s")},
                                 {"balance_w", state.at("balance_w")}});
  }
  return result;
}
std::vector<json> read_recording(const std::filesystem::path &path) {
  if (std::filesystem::file_size(path) > 64 * 1024 * 1024)
    throw std::runtime_error("Registrazione oltre 64 MiB");
  std::ifstream file(path);
  std::string line;
  std::getline(file, line);
  const auto header = json::parse(line);
  if (header.value("schema", "") != "slx.solarpilot.recording/1")
    throw std::runtime_error("Formato registrazione non compatibile");
  std::vector<json> states{header.at("initial")};
  while (std::getline(file, line)) {
    const auto row = json::parse(line);
    if (row.contains("state"))
      states.push_back(row.at("state"));
    if (states.size() > 10000)
      throw std::runtime_error("Limite 10000 snapshot superato");
  }
  auto numeric = [](const json &object, const char *key) {
    if (!object.at(key).is_number() || !std::isfinite(object.at(key).get<double>()))
      throw std::runtime_error(std::string("Campo numerico non valido: ") + key);
  };
  for (const auto &s : states) {
    if (s.at("schema") != "slx.solarpilot.state/1")
      throw std::runtime_error("Snapshot non compatibile");
    for (const char *key : {"sim_s", "hour", "pv_w", "house_w", "battery_w", "ev_w", "grid_w",
                            "soc", "ev_soc", "balance_w", "fridge_c"})
      numeric(s, key);
    for (const char *key :
         {"south_count", "module_wp",   "inverter_w",   "latitude",    "longitude",
          "tilt_deg",    "aspect_deg",  "month",        "cloud",       "temperature_c",
          "base_w",      "lighting_w",  "grid_limit_w", "capacity_wh", "reserve",
          "charge_w",    "discharge_w", "ev_limit_w",   "ev_target",   "manual_battery_w",
          "manual_ev_w"})
      numeric(s.at("config"), key);
    for (const char *key : {"north", "battery_available", "ev_connected", "auto", "drop_link",
                            "missing_measure", "rapid_clouds"})
      if (!s.at("config").at(key).is_boolean())
        throw std::runtime_error("Configurazione registrata non valida");
    if (!s.at("link").at("fresh").is_boolean() || !s.at("overload").is_boolean() ||
        !s.at("scenario_name").is_string() || !s.at("config").at("policy").is_string())
      throw std::runtime_error("Metadati registrazione non validi");
    for (const char *key : {"pv_wh", "house_wh", "ev_wh", "import_wh", "export_wh",
                            "battery_charge_wh", "battery_discharge_wh", "battery_loss_wh"})
      numeric(s.at("energy"), key);
    if (!s.at("devices").is_array() || s.at("devices").size() != 6 || !s.at("events").is_array())
      throw std::runtime_error("Dispositivi o eventi non validi");
    for (const auto &d : s.at("devices")) {
      for (const char *k : {"id", "label", "phase"})
        if (!d.at(k).is_string())
          throw std::runtime_error("Descrizione carico non valida");
      for (const char *k : {"requested", "running", "available"})
        if (!d.at(k).is_boolean())
          throw std::runtime_error("Stato carico non valido");
      for (const char *k : {"elapsed_s", "power_w", "energy_wh"})
        numeric(d, k);
    }
    for (const auto &e : s.at("events")) {
      numeric(e, "t_s");
      if (!e.at("message").is_string())
        throw std::runtime_error("Evento non valido");
    }
  }
  return states;
}
Host::Host(bool eebus) : eebus_(eebus) {
  worker_ = std::thread([this] { run(); });
}
Host::~Host() {
  stop_ = true;
  if (worker_.joinable())
    worker_.join();
}
void Host::submit(json q) {
  std::lock_guard lock(mutex_);
  if (commands_.size() < 128)
    commands_.push_back(std::move(q));
  else
    error_ = "Coda comandi piena";
}
json Host::state() {
  std::lock_guard lock(mutex_);
  return state_;
}
json Host::packets() {
  std::lock_guard lock(mutex_);
  return packets_;
}
std::vector<Sample> Host::history() {
  std::lock_guard lock(mutex_);
  return history_;
}
std::string Host::error() {
  std::lock_guard lock(mutex_);
  return error_;
}
void Host::playback(bool run, double speed) {
  std::lock_guard lock(mutex_);
  running_ = run;
  speed_ = std::clamp(speed, 1., 3600.);
}
void Host::export_csv(const std::filesystem::path &path) {
  const auto samples = history();
  std::ofstream f(path);
  if (!f)
    throw std::runtime_error("Impossibile creare CSV");
  f << "sim_s,pv_w,house_w,battery_w,ev_w,grid_w,soc\n";
  for (const auto &s : samples)
    f << s.t << ',' << s.pv << ',' << s.house << ',' << s.battery << ',' << s.ev << ',' << s.grid
      << ',' << s.soc << '\n';
}
void Host::run() {
  try {
    Client core;
    core.policy(slx_demo_policy);
    auto current = core.call({{"op", "snapshot"}});
    if (eebus_) {
      json profile = json::object();
      std::ifstream file("workspace/network-profile.json");
      if (file) { try { file >> profile; } catch (...) { profile = json::object(); } }
      if (profile.value("auto_start", false)) {
        const std::string role = profile.value("role", 0) == 1 ? "controller" : profile.value("role", 0) == 2 ? "pair" : "plant";
        const char *devices[] = {"residential", "pv-inverter", "battery", "evse"};
        current = core.call({{"op","eebus_start"},{"role",role},
          {"port",profile.value("local_ship_port",4712)}, {"lan",profile.value("lan",true)},
          {"profile",role == "pair" || profile.value("slx_profile",false) ? "slx" : "standard"},
          {"device",devices[std::clamp(profile.value("device",0),0,3)]}});
        std::ifstream trusted("workspace/network-peer-" + role + ".json");
        if (trusted) { json peer; trusted >> peer; current = core.call({{"op","eebus_pair"},{"peer",peer}}); }
      } else current = core.call({{"op", "eebus_start"}, {"role", "pair"}});
    }
    std::ofstream recording;
    size_t recorded_rows = 0, recorded_bytes = 0;
    auto write_record = [&](const json &row) {
      if (!recording.is_open())
        return;
      const auto text = row.dump();
      if (recorded_rows >= 10000 || recorded_bytes + text.size() + 1 > 64 * 1024 * 1024) {
        recording.close();
        std::lock_guard lock(mutex_);
        error_ =
            "Registrazione terminata: limite 10000 righe / 64 MiB. Per prove lunghe usare CLI.";
        return;
      }
      recording << text << '\n';
      ++recorded_rows;
      recorded_bytes += text.size() + 1;
      if (!recording.good()) {
        recording.close();
        std::lock_guard lock(mutex_);
        error_ = "Registrazione interrotta: errore di scrittura.";
      }
    };
    uint64_t last_packet = 0;
    double budget = 0, last_sample = -1;
    auto previous = std::chrono::steady_clock::now();
    {
      std::lock_guard lock(mutex_);
      state_ = current;
    }
    while (!stop_) {
      const auto now = std::chrono::steady_clock::now();
      const double elapsed = std::chrono::duration<double>(now - previous).count();
      previous = now;
      std::deque<json> queue;
      bool running;
      double speed;
      {
        std::lock_guard lock(mutex_);
        queue.swap(commands_);
        running = running_;
        speed = speed_;
      }
      for (const auto &q : queue) {
        try {
          const std::string op = q.at("op");
          if (op == "record_start") {
            const std::filesystem::path path = q.at("path").get<std::string>();
            recording.close();
            recording.open(path);
            if (!recording)
              throw std::runtime_error("Impossibile aprire registrazione");
            recorded_rows = recorded_bytes = 0;
            write_record({{"schema", "slx.solarpilot.recording/1"},
                          {"version", SLX_SOLARPILOT_VERSION},
                          {"initial", current}});
          } else if (op == "record_stop")
            recording.close();
          else {
            current = core.call(q);
            if (op == "eebus_pair") {
              const auto role = current.at("link").value("role",std::string("plant"));
              std::filesystem::create_directories("workspace");
              const auto destination = std::filesystem::path("workspace") / ("network-peer-" + role + ".json");
              const auto temporary = destination.string() + ".tmp";
              { std::ofstream file(temporary); file << q.at("peer").dump(2); if (!file) throw std::runtime_error("Impossibile salvare l'associazione"); }
#ifdef _WIN32
              if (!MoveFileExW(std::filesystem::path(temporary).c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Impossibile salvare l'associazione");
#else
              std::filesystem::rename(temporary,destination);
#endif
            }
            if (op == "eebus_start") {
              last_packet = 0;
              std::lock_guard lock(mutex_);
              packets_ = json::array();
            }
            write_record({{"request", q}, {"state", current}});
            if (op == "reset") {
              std::lock_guard lock(mutex_);
              history_.clear();
              last_sample = -1;
              budget = 0;
            }
          }
          std::lock_guard lock(mutex_);
          error_.clear();
        } catch (const std::exception &e) {
          std::lock_guard lock(mutex_);
          error_ = e.what();
        }
      }
      const bool remote_monitor = current["link"].value("role", std::string()) == "controller";
      if (running && !remote_monitor)
        budget = std::min(budget + elapsed * (current["link"].value(
                                                  "live", current["link"].value("external", false))
                                                  ? 1.0
                                                  : speed),
                          7200.);
      else
        budget = 0;
      // Limit work per pass to preserve prompt handling even at accelerated time.
      const int steps =
          std::min(current["link"].value("live", current["link"].value("external", false)) ? 1 : 30,
                   int(budget));
      if (steps > 0) {
        current = core.call({{"op", "advance"}, {"steps", steps}});
        budget -= steps;
      }
      const double t = current.at("sim_s");
      if (t != last_sample) {
        const Sample sample{t,
                            current["pv_w"],
                            current["house_w"],
                            current["battery_w"],
                            current["ev_w"],
                            current["grid_w"],
                            current["soc"]};
        {
          std::lock_guard lock(mutex_);
          if (t < last_sample)
            history_.clear();
          history_.push_back(sample);
          if (history_.size() > 4096)
            history_.erase(history_.begin());
        }
        write_record({{"state", current}});
        last_sample = t;
      }
      const auto packets = core.call({{"op", "packets"}, {"after", last_packet}});
      {
        std::lock_guard lock(mutex_);
        current["recording"] = recording.is_open();
        state_ = current;
        for (const auto &p : packets) {
          last_packet = p["id"];
          packets_.push_back(p);
        }
        while (packets_.size() > 256)
          packets_.erase(packets_.begin());
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  } catch (const std::exception &e) {
    std::lock_guard lock(mutex_);
    error_ = e.what();
  }
}
} // namespace slx::desktop
