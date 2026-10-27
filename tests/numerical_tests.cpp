// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
// Numerical oracles use fixed loads and closed-form energy calculations,
// independently of the controller's own balance_w diagnostic.
#include <cmath>
#include <iostream>
#include <random>
#include <slx/client.hpp>
using slx::json;
namespace {
int checks = 0;
void require(bool ok, const std::string &label) {
  ++checks;
  if (!ok)
    throw std::runtime_error(label);
}
double num(const json &j, const char *key) {
  return j.at(key).get<double>();
}
void near(double actual, double expected, double tolerance, const std::string &label) {
  require(std::isfinite(actual) && std::abs(actual - expected) <= tolerance,
          label + " actual=" + std::to_string(actual) + " expected=" + std::to_string(expected));
}
void config(slx::Client &c, const json &patch) {
  c.call({{"op", "configure"}, {"patch", patch}});
}
json advance(slx::Client &c, int count) {
  json s;
  while (count) {
    const int n = std::min(count, 3600);
    s = c.call({{"op", "advance"}, {"steps", n}});
    count -= n;
  }
  return s;
}
void isolated(slx::Client &c) {
  c.call({{"op", "reset"}, {"scenario", 1}});
  for (const char *id : {"fridge", "washer", "dishwasher", "oven", "cooktop", "kettle"})
    c.call({{"op", "action"}, {"device", id}, {"available", false}});
  c.call({{"op", "action"}, {"device", "clock"}, {"hour", 0.}});
  config(c, {{"base_w", 500.},
             {"lighting_w", 0.},
             {"auto", false},
             {"manual_ev_w", 0.},
             {"manual_battery_w", 0.},
             {"ev_connected", false}});
}
void ledgers(const json &a, const json &b) {
  const auto &x = a.at("energy");
  const auto &y = b.at("energy");
  auto delta = [&](const char *key) { return num(y, key) - num(x, key); };
  near(delta("pv_wh") + delta("import_wh") + delta("battery_discharge_wh"),
       delta("house_wh") + delta("ev_wh") + delta("export_wh") + delta("battery_charge_wh"), 1e-7,
       "independent cumulative AC conservation");
  const double cap = b["config"]["capacity_wh"];
  near((num(b, "soc") - num(a, "soc")) * cap,
       delta("battery_charge_wh") - delta("battery_discharge_wh") - delta("battery_loss_wh"), 1e-7,
       "DC storage conservation");
  near((num(b, "ev_soc") - num(a, "ev_soc")) * 60000, delta("ev_stored_wh"), 1e-7,
       "EV stored energy conservation");
  near(delta("ev_wh"), delta("ev_stored_wh") + delta("ev_loss_wh"), 1e-7, "EV AC/DC losses");
}
} // namespace
int main() {
  try {
    slx::Client c;
    isolated(c);
    config(c, {{"manual_battery_w", -2000.}, {"manual_ev_w", 2300.}, {"ev_connected", true}});
    auto a = c.call({{"op", "snapshot"}}), s = advance(c, 3600);
    near(num(s, "grid_w"), 4800., 1e-8, "500 W house + 2000 W charging + 2300 W EV");
    near(num(s["energy"], "house_wh"), 500., 1e-7, "one-hour household consumption");
    near(num(s["energy"], "import_wh"), 4800., 1e-7, "one-hour imported energy");
    near(num(s, "soc"), .74, 1e-11, "2000 Wh AC stores 1900 Wh DC");
    near(num(s, "ev_soc"), .45 + 2300. * .92 / 60000., 1e-11, "EV efficiency 92 percent");
    ledgers(a, s);
    isolated(c);
    config(c, {{"manual_battery_w", 1000.}});
    a = c.call({{"op", "snapshot"}});
    s = advance(c, 3600);
    near(num(s, "grid_w"), -500., 1e-8, "battery export sign");
    near(num(s["energy"], "export_wh"), 500., 1e-7, "export energy");
    near(num(s, "soc"), .55 - 1000. / .95 / 10000., 1e-11, "discharge conversion efficiency");
    ledgers(a, s);
    isolated(c);
    config(c, {{"manual_battery_w", 10000.}});
    c.call({{"op", "action"}, {"device", "battery"}, {"soc", .150001}});
    a = c.call({{"op", "snapshot"}});
    s = advance(c, 1);
    near(num(s, "battery_w"), 34.2, 1e-7, "last available DC energy limits power");
    near(num(s, "soc"), .15, 1e-12, "exact battery reserve");
    ledgers(a, s);
    isolated(c);
    config(c, {{"manual_battery_w", -10000.}});
    c.call({{"op", "action"}, {"device", "battery"}, {"soc", .999999}});
    a = c.call({{"op", "snapshot"}});
    s = advance(c, 1);
    near(num(s, "soc"), 1., 1e-12, "exact battery full SOC");
    ledgers(a, s);
    isolated(c);
    config(c, {{"ev_connected", true}, {"manual_ev_w", 3680.}, {"ev_target", .450001}});
    a = c.call({{"op", "snapshot"}});
    s = advance(c, 1);
    near(num(s, "ev_soc"), .450001, 1e-13, "EV reaches target inside final second");
    near(num(s["energy"], "ev_wh"), .000001 * 60000 / .92, 1e-10, "terminal EV AC energy");
    require(num(s, "ev_setpoint_w") >= 1380 && num(s, "ev_active_s") > 0 &&
                num(s, "ev_active_s") < 1,
            "minimum EVSE current with fractional final duration");
    ledgers(a, s);
    s = advance(c, 10);
    near(num(s, "ev_w"), 0, 1e-12, "no further charge at target");
    isolated(c);
    config(c, {{"auto", true},
               {"base_w", 2000.},
               {"grid_limit_w", 3000.},
               {"ev_connected", true},
               {"ev_mode", "fast"}});
    c.call({{"op", "action"}, {"device", "battery"}, {"soc", .150001}});
    s = advance(c, 1);
    require(!s["overload"].get<bool>() && num(s, "grid_w") <= 3000 + 1e-8,
            "EV headroom uses feasible battery power near reserve");
    isolated(c);
    config(c, {{"auto", true},
               {"battery_schedule_enabled", true},
               {"battery_schedule_start_h", 0.},
               {"battery_schedule_end_h", 0.},
               {"battery_schedule_target", .55},
               {"cloud", 0.}});
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 12.}});
    s = advance(c, 10);
    near(num(s, "soc"), .55, 1e-12, "scheduled target stays fixed even with daylight surplus");
    // Exact appliance integrals, including phase boundaries and the end of cycle.
    struct Profile {
      const char *id;
      int index, seconds;
      double wh;
    };
    for (const auto p :
         {Profile{"washer", 1, 3600, 681.6666666666666}, Profile{"dishwasher", 2, 5400, 1046.25},
          Profile{"oven", 3, 3600, 1325.6944444444443}, Profile{"cooktop", 4, 1800, 900.},
          Profile{"kettle", 5, 180, 100.}}) {
      isolated(c);
      config(c, {{"base_w", 0.}, {"battery_available", false}});
      c.call({{"op", "action"}, {"device", p.id}, {"available", true}, {"on", true}});
      s = advance(c, 60);
      c.call({{"op", "action"}, {"device", p.id}, {"on", true}});
      s = advance(c, p.seconds - 60);
      near(num(s["devices"][p.index], "energy_wh"), p.wh, 1e-7,
           std::string(p.id) + " independent complete-cycle energy");
      require(!s["devices"][p.index]["running"].get<bool>(),
              "repeated start cannot restart a cycle");
      s = advance(c, 30);
      near(num(s["devices"][p.index], "energy_wh"), p.wh, 1e-7,
           "no consumption after completed cycle");
    }
    isolated(c);
    config(c, {{"temperature_c", 45.}});
    c.call({{"op", "action"}, {"device", "fridge"}, {"available", true}});
    advance(c, 600);
    c.call({{"op", "action"}, {"device", "fridge"}, {"available", false}});
    a = c.call({{"op", "snapshot"}});
    s = advance(c, 60);
    const double ambient = s["config"]["temperature_c"];
    near(num(s, "fridge_c"),
         ambient + (num(a, "fridge_c") - ambient) * std::pow(1. - 1. / 14400, 60), 1e-10,
         "unpowered fridge has no compressor cooling");
    near(num(s["devices"][0], "power_w"), 0, 1e-12, "unpowered fridge draws zero");
    isolated(c);
    c.call({{"op", "action"}, {"device", "washer"}, {"available", true}});
    const auto before = c.call({{"op", "snapshot"}});
    bool rejected = false;
    try {
      c.call({{"op", "action"}, {"device", "washer"}, {"available", false}, {"on", true}});
    } catch (...) {
      rejected = true;
    }
    require(rejected && c.call({{"op", "snapshot"}})["devices"] == before["devices"],
            "invalid multi-field action is atomic");
    rejected = false;
    try {
      config(c, {{"washer_priority", 2.5}});
    } catch (...) {
      rejected = true;
    }
    require(rejected && c.call({{"op", "snapshot"}})["config"] == before["config"],
            "fractional priority is rejected atomically");
    isolated(c);
    config(c, {{"ev_connected", true}, {"manual_ev_w", 2300.}});
    a = c.call({{"op", "snapshot"}});
    s = advance(c, 86400);
    near(num(s, "ev_soc"), .8, 1e-12, "24-hour run reaches and holds EV target");
    near(num(s["energy"], "ev_wh"), (.8 - .45) * 60000 / .92, 1e-6,
         "full EV recharge independent energy");
    near(num(s["energy"], "house_wh"), 12000., 1e-6, "24-hour fixed household consumption");
    ledgers(a, s);
    isolated(c);
    config(c, {{"inverter_w", 20000.}, {"cloud", 0.}, {"module_wp", 300.}});
    near(num(c.call({{"op", "snapshot"}}), "pv_w"), 0., 1e-12, "PV is zero at night");
    c.call({{"op", "action"}, {"device", "clock"}, {"hour", 12.}});
    const double small = num(c.call({{"op", "snapshot"}}), "pv_w");
    config(c, {{"module_wp", 600.}});
    near(num(c.call({{"op", "snapshot"}}), "pv_w"), 2 * small, 1e-9,
         "module Wp scales linearly before clipping");
    config(c, {{"inverter_w", 1000.}});
    near(num(c.call({{"op", "snapshot"}}), "pv_w"), 1000., 1e-9, "inverter clips AC production");
    // Seeded parameter coverage exercises signs, zero/full SOC and actuator bounds.
    std::mt19937 rng(0x534c5803);
    std::uniform_real_distribution<double> unit(0., 1.);
    for (int i = 0; i < 180; ++i) {
      isolated(c);
      const double capacity = 1000 + 49000 * unit(rng), reserve = .8 * unit(rng), soc = unit(rng);
      config(c, {{"capacity_wh", capacity},
                 {"reserve", reserve},
                 {"manual_battery_w", 20000 * unit(rng) - 10000},
                 {"manual_ev_w", 11000 * unit(rng)},
                 {"ev_connected", true},
                 {"base_w", 3000 * unit(rng)},
                 {"lighting_w", 2000 * unit(rng)},
                 {"export_limit_enabled", i % 2 == 0},
                 {"export_limit_w", 500 * unit(rng)}});
      c.call({{"op", "action"}, {"device", "battery"}, {"soc", soc}});
      c.call({{"op", "action"}, {"device", "clock"}, {"hour", 24 * unit(rng)}});
      a = c.call({{"op", "snapshot"}});
      s = advance(c, 30);
      ledgers(a, s);
      near(num(s, "grid_w"),
           num(s, "house_w") + num(s, "ev_w") - num(s, "pv_w") - num(s, "battery_w"), 1e-8,
           "signed instantaneous grid");
      require(num(s, "soc") >= std::min(reserve, soc) - 1e-12 && num(s, "soc") <= 1 + 1e-12,
              "randomized SOC bound");
      if (i % 2 == 0)
        require(num(s, "grid_w") >= -s["config"]["export_limit_w"].get<double>() - 1e-7,
                "export ceiling including manual dispatch");
    }
    std::cout << checks
              << " independent numerical checks passed (closed-form profiles and 180 seeded "
                 "parameter sets)\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Numerical check " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
