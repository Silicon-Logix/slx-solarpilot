// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include <cmath>
#include <iostream>
#include <slx/client.hpp>
using slx::json;
namespace {
int checks = 0;
void require(bool condition, const char *name) {
  ++checks;
  if (!condition)
    throw std::runtime_error(name);
}
void clear_cycles(slx::Client &c) {
  for (const char *id : {"washer", "dishwasher"})
    c.call({{"op", "action"}, {"device", id}, {"on", false}});
}
void configure(slx::Client &c, const json &patch) {
  c.call({{"op", "configure"}, {"patch", patch}});
}
} // namespace
int main() {
  try {
    slx::Client c;
    c.call({{"op", "reset"}});
    clear_cycles(c);
    configure(c, {{"battery_available", false},
                  {"ev_connected", false},
                  {"export_limit_enabled", true},
                  {"export_limit_w", 0.0}});
    double available_wh = 0;
    json state;
    for (int i = 0; i < 300; ++i) {
      state = c.call({{"op", "advance"}});
      available_wh += state["pv_available_w"].get<double>() / 3600;
      require(state["grid_w"].get<double>() >= -1e-6, "zero export");
      require(std::abs(state["balance_w"].get<double>()) < 1e-6, "zero export power balance");
    }
    require(state["energy"]["curtailed_wh"].get<double>() > 100, "curtailment ledger");
    require(std::abs(state["energy"]["pv_wh"].get<double>() +
                     state["energy"]["curtailed_wh"].get<double>() - available_wh) < 1e-6,
            "available PV equals delivered plus curtailed");
    configure(c, {{"export_limit_w", 800.0}});
    state = c.call({{"op", "advance"}});
    require(std::abs(state["grid_w"].get<double>() + 800) < 1e-6, "finite export limit");
    configure(c, {{"drop_link", true}, {"export_limit_w", 0.0}});
    state = c.call({{"op", "advance"}});
    require(!state["link"]["fresh"].get<bool>() && std::abs(state["grid_w"].get<double>()) < 1e-6,
            "plant export interlock during EMS loss");
    c.call({{"op", "reset"}});
    clear_cycles(c);
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 22.0}});
    configure(c, {{"ev_mode", "fast"}, {"battery_available", false}, {"grid_limit_w", 3000.0}});
    state = c.call({{"op", "advance"}});
    require(state["ev_w"].get<double>() >= 1380 && state["grid_w"].get<double>() <= 3000,
            "fast EV respects import capacity");
    require(std::fmod(state["ev_w"].get<double>(), 230) == 0, "EV current quantization");
    configure(c, {{"ev_mode", "surplus"}});
    state = c.call({{"op", "advance"}, {"steps", 45}});
    require(state["ev_w"] == 0, "no stale hold after switching mode at night");
    configure(c,
              {{"ev_mode", "departure"}, {"ev_departure_hour", 22.02}, {"grid_limit_w", 6000.0}});
    state = c.call({{"op", "advance"}});
    require(state["ev_w"].get<double>() >= 1380, "departure mode charges without PV");
    auto command = c.call({{"op", "last_command"}});
    require(command["setpoints"]["decision"]["ev"].get<std::string>().find("non raggiungibile") !=
                std::string::npos,
            "infeasible departure explained");
    state = c.call({{"op", "advance"}, {"steps", 90}});
    require(state["ev_deadline_missed"].get<bool>(), "missed departure exposed");
    c.call({{"op", "reset"}});
    clear_cycles(c);
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 23.0}});
    c.call({{"op", "action"}, {"device", "battery"}, {"soc", .4}});
    configure(c, {{"ev_connected", false},
                  {"battery_schedule_enabled", true},
                  {"battery_schedule_start_h", 22.0},
                  {"battery_schedule_end_h", 6.0},
                  {"battery_schedule_target", .401}});
    state = c.call({{"op", "advance"}, {"steps", 120}});
    require(std::abs(state["soc"].get<double>() - .401) < 1e-8,
            "scheduled target reached without chatter");
    require(state["battery_w"] == 0, "scheduled target held within overnight window");
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 19.0}});
    state = c.call({{"op", "advance"}});
    require(state["battery_w"].get<double>() >= 0, "outside schedule no grid charge");
    c.call({{"op", "reset"}});
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 22.0}});
    configure(c, {{"battery_available", false},
                  {"ev_connected", false},
                  {"grid_limit_w", 3000.0},
                  {"dishwasher_priority", 10},
                  {"washer_priority", 1}});
    state = c.call({{"op", "advance"}});
    require(state["devices"][2]["running"].get<bool>() &&
                !state["devices"][1]["running"].get<bool>(),
            "higher priority starts first");
    state = c.call({{"op", "advance"}, {"steps", 400}});
    require(!state["devices"][1]["running"].get<bool>() && !state["overload"].get<bool>(),
            "reserve heating power of active protected cycle");
    c.call({{"op", "reset"}});
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 23.0}});
    configure(c, {{"battery_available", false},
                  {"ev_connected", false},
                  {"washer_window_enabled", true},
                  {"washer_start_h", 10.0},
                  {"washer_end_h", 18.0},
                  {"dishwasher_window_enabled", true},
                  {"dishwasher_start_h", 22.0},
                  {"dishwasher_end_h", 6.0}});
    state = c.call({{"op", "advance"}});
    require(!state["devices"][1]["running"].get<bool>() &&
                state["devices"][2]["running"].get<bool>(),
            "cross midnight load windows");
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 9.0}});
    state = c.call({{"op", "advance"}});
    require(state["devices"][2]["running"].get<bool>(), "window does not interrupt a cycle");
    c.call({{"op", "reset"}});
    clear_cycles(c);
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 22.0}});
    configure(c, {{"battery_available", false},
                  {"ev_connected", false},
                  {"tariff_enabled", true},
                  {"tariff_import_eur_kwh", .4}});
    state = c.call({{"op", "advance"}, {"steps", 3600}});
    require(std::abs(state["economics"]["import_eur"].get<double>() -
                     state["energy"]["import_wh"].get<double>() / 1000 * .4) < 1e-8,
            "explicit tariff integration");
    const auto cost = state["economics"]["net_eur"];
    configure(c, {{"tariff_enabled", false}});
    state = c.call({{"op", "advance"}, {"steps", 30}});
    require(state["economics"]["net_eur"] == cost, "disabled tariff stops accumulation");
    const auto previous = state["config"];
    bool invalid = false;
    try {
      configure(c, {{"ev_mode", "invented"}});
    } catch (...) {
      invalid = true;
    }
    require(invalid && c.call({{"op", "snapshot"}})["config"] == previous,
            "atomic invalid settings rejection");
    std::cout << checks << " EMS controls checks passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "EMS check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
