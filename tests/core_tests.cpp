// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include <cmath>
#include <iostream>
#include <slx/client.hpp>
extern "C" void SLX_SP_CALL slx_demo_policy(const slx_sp_policy_input *, slx_sp_policy_output *,
                                            void *);
namespace {
int checks = 0;
void require(bool condition, const char *message) {
  ++checks;
  if (!condition)
    throw std::runtime_error(message);
}
double number(const slx::json &s, const char *k) {
  return s.at(k).get<double>();
}
void balance(const slx::json &s) {
  require(std::abs(number(s, "pv_w") + number(s, "battery_w") + number(s, "grid_w") -
                   number(s, "house_w") - number(s, "ev_w")) < 1e-6,
          "instantaneous power balance");
  const auto &e = s["energy"];
  const double residual = number(e, "pv_wh") + number(e, "import_wh") +
                          number(e, "battery_discharge_wh") - number(e, "house_wh") -
                          number(e, "ev_wh") - number(e, "export_wh") -
                          number(e, "battery_charge_wh");
  require(std::abs(residual) < 1e-6, "integrated energy balance");
  require(number(s, "soc") >= -1e-9 && number(s, "soc") <= 1 + 1e-9, "SOC limits");
  require(number(s, "pv_w") <= s["config"]["inverter_w"].get<double>() + 1e-6, "inverter clipping");
}
} // namespace
int main() {
  try {
    require(slx_sp_abi_version() == 1, "ABI version");
    require(slx_sp_create(99) == nullptr, "ABI rejection");
    auto *handle = slx_sp_create(1);
    require(handle != nullptr, "C ABI create");
    char tiny[8]{};
    require(slx_sp_dispatch(handle, "{\"op\":\"advance\"}", 16, tiny, sizeof(tiny)) ==
                SLX_SP_BUFFER,
            "small buffer rejected before mutation");
    slx_sp_destroy(handle);
    slx::Client c;
    c.policy(slx_demo_policy);
    for (int scenario = 0; scenario < 7; ++scenario) {
      auto initial = c.call({{"op", "reset"}, {"scenario", scenario}});
      for (int k = 0; k < 120; ++k) {
        auto state = c.call({{"op", "advance"}, {"steps", 60}});
        balance(state);
        const auto &energy = state["energy"];
        const double storage = (number(state, "soc") - number(initial, "soc")) * 10000;
        require(std::abs(storage - number(energy, "battery_charge_wh") +
                         number(energy, "battery_discharge_wh") +
                         number(energy, "battery_loss_wh")) < 1e-6,
                "DC storage and conversion loss balance");
      }
      auto s = c.call({{"op", "snapshot"}});
      require(s["link"]["exchanges"].get<int>() > 0, "real TCP exchange");
      if (scenario == 0) {
        require(s["devices"][1]["running"] == false, "washer completes");
        require(s["devices"][2]["running"] == false, "dishwasher completes");
        require(s["devices"][1]["energy_wh"].get<double>() > 500, "washer multiphase energy");
      }
      std::cout << "Scenario " << scenario << ": 7200 fixed steps OK\n";
    }
    c.call({{"op", "reset"}});
    auto first = c.call({{"op", "advance"}, {"steps", 120}});
    c.call({{"op", "reset"}});
    auto second = c.call({{"op", "advance"}, {"steps", 120}});
    require(first["energy"] == second["energy"] && first["soc"] == second["soc"],
            "deterministic repeat");
    auto packets = c.call({{"op", "packets"}});
    bool tx = false, rx = false, ack = false;
    for (const auto &p : packets) {
      tx |= p["direction"] == "PC TX";
      rx |= p["direction"] == "EMS RX";
      if (p["payload"].contains("previous_ack") && !p["payload"]["previous_ack"].is_null())
        ack |= p["payload"]["previous_ack"]["status"] == "executed";
    }
    require(tx && rx && ack, "captured CBOR send/receive and executed ACK");
    auto cmd = c.call({{"op", "last_command"}});
    require(c.call({{"op", "verify_command"}, {"command", cmd}})["status"] ==
                "rejected:duplicate-or-order",
            "duplicate command rejected");
    auto expired = cmd;
    expired["deadline_real_ms"] = 0;
    require(c.call({{"op", "verify_command"}, {"command", expired}})["status"] ==
                "rejected:expired",
            "expired command rejected");
    auto wrong = cmd;
    wrong["session"] = "old-session";
    require(c.call({{"op", "verify_command"}, {"command", wrong}})["status"] ==
                "rejected:session-or-version",
            "old session rejected");
    c.call({{"op", "reconnect"}});
    require(c.call({{"op", "verify_command"}, {"command", cmd}})["status"] ==
                "rejected:session-or-version",
            "reconnection invalidates command");
    c.call({{"op", "configure"}, {"patch", {{"drop_link", true}}}});
    auto down = c.call({{"op", "advance"}, {"steps", 30}});
    require(!down["link"]["fresh"].get<bool>() && down["ev_w"] == 0 && down["battery_w"] == 0,
            "disconnect fallback stops flexible loads");
    require(down["devices"][1]["running"] == true, "protected wash survives communication loss");
    c.call({{"op", "configure"}, {"patch", {{"drop_link", false}, {"missing_measure", true}}}});
    auto missing = c.call({{"op", "advance"}});
    require(!missing["link"]["fresh"].get<bool>() && missing["battery_w"] == 0,
            "missing measurements safe fallback");
    c.call({{"op", "configure"}, {"patch", {{"missing_measure", false}}}});
    require(c.call({{"op", "advance"}})["link"]["fresh"].get<bool>(), "link recovery");
    c.call({{"op", "reset"}, {"scenario", 4}});
    c.call({{"op", "configure"},
            {"patch", {{"auto", false}, {"manual_battery_w", 10000.0}, {"cloud", 1.0}}}});
    auto reserve = c.call({{"op", "advance"}, {"steps", 3600}});
    require(number(reserve, "soc") >= .15 - 1e-9, "battery reserve under forced discharge");
    c.call({{"op", "action"}, {"device", "battery"}, {"soc", .999999}});
    c.call({{"op", "configure"}, {"patch", {{"manual_battery_w", -10000.0}}}});
    auto full = c.call({{"op", "advance"}, {"steps", 60}});
    require(number(full, "soc") <= 1, "charge saturation");
    c.call({{"op", "configure"}, {"patch", {{"ev_connected", false}, {"manual_ev_w", 11000.0}}}});
    require(c.call({{"op", "advance"}})["ev_w"] == 0, "unplugged EV cannot charge");
    bool invalid = false;
    try {
      c.call({{"op", "configure"}, {"patch", {{"south_count", 11}}}});
    } catch (...) {
      invalid = true;
    }
    require(invalid, "unsupported PV wiring rejected");
    c.call({{"op", "reset"}});
    c.call({{"op", "configure"}, {"patch", {{"policy", "custom"}}}});
    auto custom = c.call({{"op", "advance"}});
    require(number(custom, "ev_w") > 0, "public policy changes allocation");
    c.call({{"op", "reset"}});
    c.call({{"op", "configure"},
            {"patch", {{"auto", false}, {"manual_ev_w", 3680.0}, {"manual_battery_w", 0.0}}}});
    auto manual = c.call({{"op", "advance"}});
    balance(manual);
    require(number(manual, "ev_w") == 3680, "manual EV command confirmed");
    std::cout << checks << " checks passed, real NNG TCP / CBOR\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
