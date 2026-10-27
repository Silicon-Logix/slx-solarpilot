// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include "ems_panels.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <imgui.h>
namespace slx::desktop {
namespace {
const ImVec4 orange{.72f, .39f, .08f, 1};
void title(const char *text, const char *description) {
  ImGui::PushFont(nullptr, 25);
  ImGui::TextUnformatted(text);
  ImGui::PopFont();
  ImGui::TextWrapped("%s", description);
  ImGui::Spacing();
}
void set(Host &host, const char *key, const json &value) {
  host.submit({{"op", "configure"}, {"patch", {{key, value}}}});
}
void flag(Host &host, const json &cfg, const char *label, const char *key) {
  bool value = cfg.at(key);
  if (ImGui::Checkbox(label, &value))
    set(host, key, value);
}
void number(Host &host, const json &cfg, const char *label, const char *key, float lo, float hi,
            const char *format) {
  ImGui::PushID(key);
  float value = cfg.at(key);
  ImGui::TextUnformatted(label);
  ImGui::SetNextItemWidth(-1);
  if (ImGui::SliderFloat("##value", &value, lo, hi, format))
    set(host, key, value);
  ImGui::PopID();
}
} // namespace
void ems_settings_panel(Host &host, const json &state) {
  title("Strategie del controllore EMS",
        "Strategie dell'EMS locale. Per un controllore esterno usare le regolazioni "
        "del dispositivo collegato. Le potenze applicate restano osservabili nel monitor.");
  const auto &c = state.at("config");
  if (ImGui::BeginTable("ems_settings", 2,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextColumn();
    ImGui::SeparatorText("Rete e limitazione dell'immissione");
    flag(host, c, "Limita immissione / zero export", "export_limit_enabled");
    number(host, c, "Massima immissione in rete", "export_limit_w", 0, 10000, "%.0f W");
    number(host, c, "Obiettivo prelievo minimo", "grid_setpoint_w", 0, 500, "%.0f W");
    number(host, c, "Limite prelievo domestico", "grid_limit_w", 1000, 20000, "%.0f W");
    ImGui::Text("PV disponibile %.0f W | taglio %.0f W", state.value("pv_available_w", 0.),
                state.value("curtailed_w", 0.));
    ImGui::TextWrapped("Il surplus non utilizzabile viene limitato dall'inverter. La potenza "
                       "limitata e contabilizzata separatamente. L'interblocco locale "
                       "dell'impianto conserva il limite anche senza EMS.");
    ImGui::SeparatorText("Accumulo e programmazione oraria");
    flag(host, c, "Carica programmata anche dalla rete", "battery_schedule_enabled");
    number(host, c, "Inizio finestra", "battery_schedule_start_h", 0, 23.99f, "%.2f h");
    number(host, c, "Fine finestra", "battery_schedule_end_h", 0, 23.99f, "%.2f h");
    number(host, c, "SOC minimo da raggiungere nella finestra", "battery_schedule_target", 0, 1,
           "%.2f");
    number(host, c, "Riserva per autoconsumo", "reserve", 0, .9f, "%.2f");
    ImGui::TextWrapped("Le finestre possono attraversare mezzanotte; inizio e fine uguali indicano "
                       "l'intera giornata. La carica programmata rispetta il limite di rete e non "
                       "costituisce un'ottimizzazione tariffaria.");
    ImGui::TableNextColumn();
    ImGui::SeparatorText("Ricarica EV");
    int mode = c["ev_mode"] == "fast" ? 1 : c["ev_mode"] == "departure" ? 2 : 0;
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo(
            "##EVmode", &mode,
            "Surplus fotovoltaico\0Rapida / rete consentita\0Obiettivo SOC entro partenza\0"))
      set(host, "ev_mode", mode == 1 ? "fast" : mode == 2 ? "departure" : "surplus");
    number(host, c, "SOC obiettivo EV", "ev_target", .1f, 1, "%.2f");
    number(host, c, "Ora di partenza (prossima occorrenza)", "ev_departure_hour", 0, 23.99f,
           "%.2f h");
    number(host, c, "Limite wallbox", "ev_limit_w", 1380, 11000, "%.0f W");
    flag(host, c, "Regolazione a passi di 1 A / 230 V sintetici", "ev_quantize");
    number(host, c, "Margine di avvio sul minimo 1380 W", "ev_start_margin_w", 0, 1000, "%.0f W");
    number(host, c, "Mantieni minimo durante nuvole brevi", "ev_stop_delay_s", 0, 300, "%.0f s");
    if (mode == 2) {
      ImGui::Text("Tempo alla partenza %.1f min", state.value("ev_departure_remaining_s", 0.) / 60);
      if (state.value("ev_deadline_missed", false))
        ImGui::TextColored(orange, "Obiettivo non raggiunto entro la partenza");
    }
    ImGui::SeparatorText("Stima con tariffe esplicite");
    flag(host, c, "Calcola costi dimostrativi con tariffe fisse", "tariff_enabled");
    number(host, c, "Tariffa prelievo ipotizzata", "tariff_import_eur_kwh", 0, 1, "%.3f EUR/kWh");
    number(host, c, "Corrispettivo immissione ipotizzato", "tariff_export_eur_kwh", 0, 1,
           "%.3f EUR/kWh");
    if (state.contains("economics") && c["tariff_enabled"].get<bool>())
      ImGui::Text("Costo %.3f EUR | credito %.3f EUR",
                  state["economics"]["import_eur"].get<double>(),
                  state["economics"]["export_eur"].get<double>());
    ImGui::TextWrapped("Ipotesi inserite dall'operatore, senza imposte, quote fisse o prezzi live. "
                       "Per un confronto pulito resettare lo scenario prima di ogni prova.");
    ImGui::EndTable();
  }
  ImGui::SeparatorText("Priorita e finestre di avvio dei cicli");
  if (ImGui::BeginTable("load_windows", 2, ImGuiTableFlags_Resizable)) {
    for (const char *id : {"washer", "dishwasher"}) {
      const std::string name = id;
      ImGui::TableNextColumn();
      ImGui::PushID(id);
      ImGui::TextUnformatted(name == "washer" ? "Lavatrice" : "Lavastoviglie");
      flag(host, c, "Abilita finestra di avvio", (name + "_window_enabled").c_str());
      number(host, c, "Avvio consentito dalle", (name + "_start_h").c_str(), 0, 23.99f, "%.2f h");
      number(host, c, "Avvio consentito fino alle", (name + "_end_h").c_str(), 0, 23.99f, "%.2f h");
      int priority = c.at(name + "_priority");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::SliderInt("Priorita", &priority, 1, 10))
        set(host, (name + "_priority").c_str(), priority);
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  ImGui::TextWrapped("Numero piu alto = priorita maggiore. Le finestre autorizzano l'avvio, senza "
                     "interrompere un ciclo gia attivo. Un carico in attesa richiede anche margine "
                     "per la sua fase di riscaldamento.");
  ImGui::SeparatorText("Bilancio ed energia applicata");
  const auto &energy = state.at("energy");
  if (ImGui::BeginTable("energy-ledger", 4,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
    for (const auto &row :
         {std::pair{"PV prodotto", "pv_wh"}, std::pair{"Consumi casa", "house_wh"},
          std::pair{"EV lato AC", "ev_wh"}, std::pair{"Prelievo rete", "import_wh"},
          std::pair{"Immissione rete", "export_wh"},
          std::pair{"Carica accumulo AC", "battery_charge_wh"},
          std::pair{"Scarica accumulo AC", "battery_discharge_wh"},
          std::pair{"Perdite accumulo", "battery_loss_wh"},
          std::pair{"EV immagazzinata DC", "ev_stored_wh"},
          std::pair{"Perdite ricarica EV", "ev_loss_wh"},
          std::pair{"PV non prodotto per limite", "curtailed_wh"}}) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(row.first);
      ImGui::TableNextColumn();
      if (energy.contains(row.second))
        ImGui::Text("%.3f Wh", energy.at(row.second).get<double>());
      else
        ImGui::TextDisabled("n/d");
    }
    ImGui::EndTable();
  }
  ImGui::Text("Residuo del bilancio di potenza: %.9f W", state.at("balance_w").get<double>());
  if (state.value("ev_active_s", 0.) > 0 && state.value("ev_active_s", 0.) < 1)
    ImGui::TextWrapped("EV nell'ultimo secondo: %.0f W attivi per %.6f s, %.3f W medi "
                       "contabilizzati. Arresto al SOC obiettivo.",
                       state.value("ev_setpoint_w", 0.), state.value("ev_active_s", 0.),
                       state.at("ev_w").get<double>());
}
void controller_bench_panel(Host &host, const json &state, const std::filesystem::path &root) {
  title(
      "Rete e dispositivi",
      "Collega un controllore all'impianto simulato o esplora dispositivi compatibili sulla rete.");
  auto send = [&](json q) { host.submit({{"op", "network_request"}, {"request", std::move(q)}}); };
  const auto net = state.value("network", json::object());
  const auto status = net.value("status", json::object());
  static int role = 0, profile = 0, local_ship_port = 4712;
  static bool lan = true, slx_profile = false;
  static bool auto_start = true;
  static char ip[64] = "127.0.0.1", bind[64] = "0.0.0.0";
  static int port = 4712, local_port = 1502, remote_port = 502, unit = 1;
  static bool loaded = false;
  static std::string profile_status;
  const auto profile_path = root / "workspace/network-profile.json";
  if (!loaded) {
    loaded = true;
    if (std::filesystem::exists(profile_path))
      try {
        std::ifstream file(profile_path);
        const auto saved = json::parse(file);
        role = std::clamp(saved.value("role", 0), 0, 2);
        profile = std::clamp(saved.value("device", 0), 0, 3);
        lan = saved.value("lan", true);
        slx_profile = saved.value("slx_profile", false);
        auto_start = saved.value("auto_start", true);
        local_ship_port = saved.value("local_ship_port", 4712);
        port = saved.value("ship_port", 4712);
        local_port = saved.value("local_port", 1502);
        remote_port = saved.value("remote_port", 502);
        unit = saved.value("unit", 1);
        std::strncpy(ip, saved.value("ip", std::string("127.0.0.1")).c_str(), sizeof(ip) - 1);
        std::strncpy(bind, saved.value("bind", std::string("0.0.0.0")).c_str(), sizeof(bind) - 1);
      } catch (const std::exception &) {
        profile_status = "Profilo di rete non leggibile.";
      }
  }
  if (ImGui::Button("Salva impostazioni di rete"))
    try {
      std::filesystem::create_directories(profile_path.parent_path());
      std::ofstream file(profile_path);
      if (!file)
        throw std::runtime_error("Salvataggio non riuscito");
      file << json({{"role", role},
                    {"device", profile},
                    {"lan", lan},
                    {"slx_profile", slx_profile},
                    {"auto_start", auto_start},
                    {"ip", ip},
                    {"bind", bind},
                    {"ship_port", port},
                    {"local_ship_port", local_ship_port},
                    {"local_port", local_port},
                    {"remote_port", remote_port},
                    {"unit", unit}})
                  .dump(2);
      std::ofstream bench(root / "workspace/bench-profile.json");
      if (!bench)
        throw std::runtime_error("Salvataggio configurazione EMS non riuscito");
      bench << json({{"schema", "slx.solarpilot.bench-profile/1"},
                     {"config", state.at("config")},
                     {"clock_hour", state.at("hour")},
                     {"endpoint", "SHIP"}})
                   .dump(2);
      profile_status = "Impostazioni salvate.";
    } catch (const std::exception &e) {
      profile_status = e.what();
    }
  if (!profile_status.empty()) {
    ImGui::SameLine();
    ImGui::TextUnformatted(profile_status.c_str());
  }

  ImGui::Checkbox("Ripristina collegamento e simulazione all'avvio", &auto_start);
  ImGui::Combo("Ruolo", &role,
               "Impianto per EMS esterno\0EMS per dispositivo esterno\0Impianto e EMS locali\0");
  ImGui::Combo("Dispositivo EEBUS", &profile,
               "Residenziale\0Inverter fotovoltaico\0Accumulo\0Wallbox\0");
  ImGui::Checkbox("Profilo SLX esteso / impianto completo", &slx_profile);
  ImGui::InputInt("Porta SHIP locale", &local_ship_port);
  ImGui::Checkbox("Ricerca e pubblicazione sulla LAN (mDNS)", &lan);
  if (ImGui::Button("Avvia collegamento EEBUS")) {
    const char *devices[]{"residential", "pv-inverter", "battery", "evse"};
    host.submit({{"op", "eebus_start"},
                 {"role", role == 1   ? "controller"
                          : role == 2 ? "pair"
                                      : "plant"},
                 {"port", local_ship_port},
                 {"profile", role == 2 || slx_profile ? "slx" : "standard"},
                 {"device", devices[profile]},
                 {"lan", lan}});
    send({{"op", "network_status"}});
  }
  ImGui::SameLine();
  if (ImGui::Button("Aggiorna rete"))
    send({{"op", "network_status"}});
  const auto identity = state.at("link").value("identity", json::object());
  for (const char *name : {"plant", "controller"})
    if (identity.contains(name)) {
      const auto &id = identity.at(name);
      ImGui::Text("%s: porta %d", name, id.value("port", 0));
      ImGui::TextWrapped("SKI: %s", id.value("ski", std::string()).c_str());
      if (ImGui::SmallButton((std::string("Copia identità##") + name).c_str()))
        ImGui::SetClipboardText(id.dump(2).c_str());
    }
  ImGui::InputText("IP del dispositivo", ip, sizeof(ip));
  if (ImGui::BeginTabBar("Network protocols")) {
    if (ImGui::BeginTabItem("EEBUS / SHIP")) {
      ImGui::TextUnformatted(status.value("connected", false) ? "EEBUS connesso"
                                                              : "EEBUS in attesa di associazione");
      if (ImGui::Button("Disconnetti EEBUS"))
        send({{"op", "disconnect"}});
      ImGui::SameLine();
      if (ImGui::Button("Riconnetti EEBUS"))
        send({{"op", "reconnect"}});
      ImGui::InputInt("Porta SHIP", &port);
      if (ImGui::Button("Leggi identità da IP"))
        send({{"op", "probe"}, {"host", ip}, {"port", port}});
      static json peer = nullptr;
      static std::string last_probe;
      if (net.contains("probe") && net["probe"].dump() != last_probe) {
        peer = net["probe"];
        last_probe = peer.dump();
      }
      const auto peers = status.value("peers", json::array());
      for (const auto &p : peers) {
        const auto label = p.value("brand", std::string()) + " " + p.value("model", std::string()) +
                           "##" + p.at("ski").get<std::string>();
        if (ImGui::Selectable(label.c_str())) {
          peer = p;
          if (p.contains("addresses") && !p["addresses"].empty())
            peer["host"] = p["addresses"][0];
          peer["fingerprint"] = "";
        }
      }
      if (slx_profile && ImGui::Button("Carica identità EMS di riferimento"))
        try {
          std::ifstream file(root / "workspace/eebus/controller-peer.json");
          peer = json::parse(file);
        } catch (const std::exception &) {
          profile_status = "Avvia prima il servizio EMS di riferimento.";
        }
      if (!peer.is_null()) {
        ImGui::TextWrapped("Identità remota: %s", peer.value("ski", std::string()).c_str());
        ImGui::TextWrapped(
            "Verifica lo SKI sul dispositivo e autorizza l'associazione anche sull'altro sistema.");
        if (ImGui::Button("Associa identità selezionata")) {
          host.submit({{"op", "eebus_pair"}, {"peer", peer}});
        }
      }
      if (ImGui::Button("Leggi dispositivi EEBUS"))
        send({{"op", "devices"}});
      static std::string selected_entity;
      for (const auto &d : net.value("devices", json::array())) {
        const auto key = d.at("key").get<std::string>();
        const auto label =
            d.value("label", d.value("type", std::string("Dispositivo"))) + "##" + key;
        if (ImGui::Selectable(label.c_str(), selected_entity == key))
          selected_entity = key;
        if (d.contains("data"))
          for (const auto &[k, v] : d["data"].items())
            if (v.is_number()) {
              const auto text = k + ": " + v.dump();
              ImGui::TextUnformatted(text.c_str());
            }
      }
      if (!selected_entity.empty()) {
        static int direction = 0, duration = 7200;
        static float watts = 1000;
        ImGui::Combo("Limite", &direction, "Assorbimento LPC\0Produzione LPP\0");
        ImGui::InputFloat("Potenza [W]", &watts);
        ImGui::InputInt("Validità [s]", &duration);
        if (ImGui::Button("Invia limite EEBUS"))
          send({{"op", "write_limit"},
                {"entity", selected_entity},
                {"direction", direction ? "produce" : "consume"},
                {"value_w", watts},
                {"duration_s", duration}});
        if (net.contains("write_limit"))
          ImGui::TextUnformatted(net["write_limit"].value("accepted", false)
                                     ? "Comando accettato dal dispositivo"
                                     : "Comando in attesa");
      }
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("SunSpec / Modbus TCP")) {
      ImGui::BeginDisabled(state.at("link").value("role", std::string()) == "controller");
      ImGui::InputText("IP locale di ascolto", bind, sizeof(bind));
      ImGui::InputInt("Porta Modbus locale", &local_port);
      if (ImGui::Button("Esponi impianto SunSpec"))
        send({{"op", "sunspec_start"},
              {"address", std::string(bind) + ":" + std::to_string(local_port)}});
      ImGui::SameLine();
      if (ImGui::Button("Arresta SunSpec"))
        send({{"op", "sunspec_stop"}});
      ImGui::EndDisabled();
      if (status.contains("sunspec") && status["sunspec"].is_object()) {
        ImGui::Text("In ascolto: %s", status["sunspec"].value("address", std::string()).c_str());
        ImGui::TextWrapped("Unità 1: inverter; 2: contatore rete; 3: accumulo. Base registri "
                           "40000. Unità 10: estensioni SLX facoltative per EV e carichi.");
      }
      ImGui::InputInt("Porta Modbus remota", &remote_port);
      ImGui::InputInt("Unità remota", &unit);
      auto remote = [&](const char *op) {
        return json{{"op", op}, {"host", ip}, {"port", remote_port}, {"unit", unit}};
      };
      if (ImGui::Button("Leggi dispositivo SunSpec da IP"))
        send(remote("sunspec_scan"));
      json scan = net.value("sunspec_latest", net.value("sunspec_scan", json::object()));
      static int model = 123;
      static char point[64] = "WMaxLimPct";
      static float value = 50;
      for (const auto &m : scan.value("models", json::array())) {
        const auto label =
            std::to_string(m.at("id").get<int>()) + " / " + m.value("name", std::string());
        if (ImGui::TreeNode(label.c_str())) {
          for (const auto &[key, v] : m.at("points").items()) {
            if (v.is_null())
              continue;
            ImGui::Text("%s: %s", key.c_str(), v.dump().c_str());
          }
          for (const auto &control : m.value("controls", json::array())) {
            const auto text = control.get<std::string>();
            if (ImGui::SmallButton(("Regola " + text).c_str())) {
              model = m.at("id");
              std::strncpy(point, text.c_str(), sizeof(point) - 1);
            }
          }
          ImGui::TreePop();
        }
      }
      ImGui::InputInt("Modello", &model);
      ImGui::InputText("Parametro", point, sizeof(point));
      ImGui::InputFloat("Valore in unità fisiche", &value);
      if (ImGui::Button("Scrivi parametro SunSpec")) {
        auto q = remote("sunspec_write");
        q["model"] = model;
        q["point"] = point;
        q["value"] = value;
        send(q);
      }
      ImGui::TextWrapped("Lettura dei modelli dichiarati dal dispositivo. I comandi disponibili "
                         "dipendono dalle funzioni e dai permessi del costruttore.");
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}
} // namespace slx::desktop
