// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include "ems_panels.hpp"
#include "gl_api.hpp"
#include "host.hpp"
#include "paths.hpp"
#include "product_panels.hpp"
#include "presentation.hpp"
#include "renderer.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <future>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <implot.h>
#include <iostream>
#include <slx/version.hpp>
#ifdef _WIN32
#include <stb_image.h>
#include <windows.h>
#endif
namespace slx::desktop {
namespace {
const ImVec4 ink{.098f, .145f, .255f, 1}, muted{.34f, .39f, .48f, 1}, accent{.325f, .329f, .91f, 1};
const ImVec4 green{.03f, .47f, .38f, 1}, orange{.72f, .39f, .08f, 1}, red{.72f, .22f, .28f, 1};
const char *pages[]{"Panoramica",           "Carichi domestici",  "Impianto e PVGIS",
                    "Scenari di prova",     "Monitor protocollo", "Registrazioni",
                    "About / informazioni", "Strategie EMS",      "Rete e dispositivi",
                    "Su richiesta"};
constexpr int page_count = 10;
json read_json(const std::filesystem::path &p) {
  std::ifstream f(p);
  if (!f)
    throw std::runtime_error("File non disponibile: " + p.string());
  return json::parse(f);
}
void theme() {
  ImGui::StyleColorsLight();
  auto &s = ImGui::GetStyle();
  s.WindowRounding = 0;
  s.ChildRounding = 8;
  s.FrameRounding = 5;
  s.PopupRounding = 7;
  s.GrabRounding = 4;
  s.WindowPadding = {18, 14};
  s.FramePadding = {10, 7};
  s.ItemSpacing = {10, 9};
  s.WindowBorderSize = 0;
  s.ChildBorderSize = 1;
  s.ScrollbarSize = 12;
  auto *c = s.Colors;
  c[ImGuiCol_Text] = ink;
  c[ImGuiCol_TextDisabled] = muted;
  c[ImGuiCol_WindowBg] = {.953f, .961f, .976f, 1};
  c[ImGuiCol_ChildBg] = {1, 1, 1, 1};
  c[ImGuiCol_Border] = {.88f, .9f, .94f, 1};
  c[ImGuiCol_FrameBg] = {.94f, .95f, .98f, 1};
  c[ImGuiCol_FrameBgHovered] = {.89f, .91f, .99f, 1};
  c[ImGuiCol_Button] = {.91f, .92f, .98f, 1};
  c[ImGuiCol_ButtonHovered] = {.83f, .85f, .98f, 1};
  c[ImGuiCol_ButtonActive] = accent;
  c[ImGuiCol_CheckMark] = accent;
  c[ImGuiCol_SliderGrab] = accent;
  c[ImGuiCol_Header] = {.90f, .91f, .99f, 1};
  c[ImGuiCol_HeaderHovered] = {.84f, .86f, .98f, 1};
  c[ImGuiCol_HeaderActive] = {.8f, .82f, .96f, 1};
  c[ImGuiCol_TabSelected] = {.86f, .88f, .98f, 1};
  c[ImGuiCol_PlotLines] = accent;
  ImPlot::StyleColorsLight();
  ImPlot::GetStyle().PlotPadding = {12, 10};
}
void heading(const char *title, const char *subtitle) {
  ImGui::PushFont(nullptr, 25);
  ImGui::TextUnformatted(title);
  ImGui::PopFont();
  ImGui::TextDisabled("%s", subtitle);
  ImGui::Spacing();
}
void metric(const char *id, const char *label, double value, const char *unit, ImVec4 color,
            float width) {
  ImGui::BeginChild(id, {width, 96}, ImGuiChildFlags_Borders);
  ImGui::TextDisabled("%s", label);
  ImGui::PushFont(nullptr, 26);
  ImGui::TextColored(color, "%.2f", value);
  ImGui::PopFont();
  ImGui::SameLine();
  ImGui::TextDisabled("%s", unit);
  ImGui::EndChild();
}
void patch(Host &host, const char *key, json value) {
  host.submit({{"op", "configure"}, {"patch", {{key, std::move(value)}}}});
}
bool slider(Host &host, const json &cfg, const char *label, const char *key, float lo, float hi,
            const char *fmt) {
  float v = cfg.at(key);
  ImGui::PushID(key);
  ImGui::TextUnformatted(label);
  ImGui::SetNextItemWidth(-1);
  const bool changed = ImGui::SliderFloat("##value", &v, lo, hi, fmt);
  if (changed)
    patch(host, key, v);
  ImGui::PopID();
  return changed;
}
void check(Host &host, const json &cfg, const char *label, const char *key) {
  bool v = cfg.at(key);
  if (ImGui::Checkbox(label, &v))
    patch(host, key, v);
}
void chart(const std::vector<Sample> &history, float height) {
  if (ImPlot::BeginPlot("##history", {-1, height}, ImPlotFlags_NoTitle)) {
    ImPlot::SetupAxes("Tempo simulato [s]", "Potenza [W]", ImPlotAxisFlags_AutoFit,
                      ImPlotAxisFlags_AutoFit);
    ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_Horizontal);
    if (!history.empty()) {
      ImPlot::SetNextLineStyle(green, 2);
      ImPlot::PlotLine("Fotovoltaico", &history[0].t, &history[0].pv, int(history.size()), 0, 0,
                       sizeof(Sample));
      ImPlot::SetNextLineStyle(orange, 2);
      ImPlot::PlotLine("Casa", &history[0].t, &history[0].house, int(history.size()), 0, 0,
                       sizeof(Sample));
      ImPlot::SetNextLineStyle(accent, 2);
      ImPlot::PlotLine("Rete (+ prelievo)", &history[0].t, &history[0].grid, int(history.size()), 0,
                       0, sizeof(Sample));
      ImPlot::SetNextLineStyle({.2f, .61f, .73f, 1}, 1.5f);
      ImPlot::PlotLine("EV", &history[0].t, &history[0].ev, int(history.size()), 0, 0,
                       sizeof(Sample));
    }
    ImPlot::EndPlot();
  }
}
void loads_panel(Host &host, const json &s, const std::string &selected = {}) {
  heading("Carichi domestici", "Richieste operatore e stato confermato dal modello");
  for (const auto &load : s.at("devices")) {
    const std::string id = load["id"], name = load["label"];
    ImGui::PushID(id.c_str());
    if (id == selected)
      ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{.94f, .94f, 1, 1});
    ImGui::BeginChild("load", {0, 116}, ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(name.c_str());
    ImGui::SameLine();
    ImGui::TextColored(green, "%.0f W", load["power_w"].get<double>());
    ImGui::TextDisabled("%s | %.0f s | %.1f Wh", load["phase"].get<std::string>().c_str(),
                        load["elapsed_s"].get<double>(), load["energy_wh"].get<double>());
    bool available = load["available"];
    if (ImGui::Checkbox("Disponibile", &available))
      host.submit({{"op", "action"}, {"device", id}, {"available", available}});
    if (id != "fridge") {
      ImGui::SameLine();
      const bool active = load["running"].get<bool>() || load["requested"].get<bool>();
      const bool protected_cycle =
          load["running"].get<bool>() && (id == "washer" || id == "dishwasher");
      ImGui::BeginDisabled(!available || protected_cycle);
      if (ImGui::Button(active ? "Spegni / annulla" : "Richiedi avvio"))
        host.submit({{"op", "action"}, {"device", id}, {"on", !active}});
      ImGui::EndDisabled();
      if (protected_cycle) {
        ImGui::SameLine();
        ImGui::TextDisabled("Ciclo protetto");
      }
    }
    ImGui::EndChild();
    if (id == selected)
      ImGui::PopStyleColor();
    ImGui::PopID();
  }
  ImGui::TextWrapped("Il frigorifero usa un termostato sintetico (%.1f C). I cicli gia avviati "
                     "proseguono anche durante la perdita del collegamento EMS.",
                     s["fridge_c"].get<double>());
}
void controls(Host &host, const json &s, const std::string &selected, json &mechanism_pose,
              const json &appliances) {
  const auto &c = s.at("config");
  heading("Controllo energetico", s["link"].value("external", false)
                                      ? "Impianto PC / controllore esterno"
                                      : "EMS locale su collegamento EEBUS reale");
  check(host, c, "Gestione automatica EMS", "auto");
  int policy = c["policy"] == "custom" ? 1 : 0;
  ImGui::TextDisabled("Strategia di gestione");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::Combo("##Strategia", &policy, "Bilanciata SLX\0Policy pubblica / priorita EV\0"))
    patch(host, "policy", policy ? "custom" : "balanced");
  if (!selected.empty()) {
    ImGui::SeparatorText("Oggetto selezionato");
    ImGui::TextColored(accent, "%s", selected.c_str());
    for (const auto &l : s["devices"])
      if (l["id"] == selected) {
        ImGui::Text("%s | %.0f W", l["label"].get<std::string>().c_str(),
                    l["power_w"].get<double>());
        bool on = l["requested"].get<bool>() || l["running"].get<bool>();
        bool locked =
            l["running"].get<bool>() && (selected == "washer" || selected == "dishwasher");
        ImGui::BeginDisabled(locked);
        if (ImGui::Button(on ? "Spegni / annulla" : "Richiedi avvio"))
          host.submit({{"op", "action"}, {"device", selected}, {"on", !on}});
        ImGui::EndDisabled();
      }
  }
  const auto device = appliances["devices"].find(selected + "_root");
  if (device != appliances["devices"].end()) {
    for (const auto &a : (*device)["articulations"]) {
      if (a["role"] == "washer_drum_rotation")
        continue;
      const std::string node = a["node"];
      float angle = mechanism_pose.value(node, 0.f);
      bool running = false;
      for (const auto &l : s["devices"])
        if (l["id"] == selected)
          running = l["running"].get<bool>();
      ImGui::BeginDisabled(running);
      ImGui::TextDisabled("Apertura sportello");
      ImGui::SetNextItemWidth(-1);
      if (ImGui::SliderFloat(node.c_str(), &angle, a["min_deg"].get<float>(),
                             a["max_deg"].get<float>(), "%.0f deg"))
        mechanism_pose[node] = angle;
      ImGui::EndDisabled();
    }
  }
  ImGui::SeparatorText("Condizioni ambientali");
  float hour = s.at("hour");
  ImGui::TextUnformatted("Ora locale standard (senza DST)");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::SliderFloat("##Ora", &hour, 0, 23.99f, "%.2f h"))
    host.submit({{"op", "action"}, {"device", "clock"}, {"hour", hour}});
  int month = c["month"];
  ImGui::TextUnformatted("Mese");
  ImGui::SetNextItemWidth(-1);
  if (ImGui::SliderInt("##Mese", &month, 1, 12))
    patch(host, "month", month);
  slider(host, c, "Copertura nuvole", "cloud", 0, 1, "%.2f");
  check(host, c, "Nuvole in rapido passaggio", "rapid_clouds");
  slider(host, c, "Temperatura", "temperature_c", -15, 45, "%.0f C");
  slider(host, c, "Consumi di base", "base_w", 0, 3000, "%.0f W");
  slider(host, c, "Illuminazione", "lighting_w", 0, 2000, "%.0f W");
  ImGui::SeparatorText("Accumulo ed EV");
  ImGui::Text("Batteria %.1f %% | EV %.1f %%", s["soc"].get<double>() * 100,
              s["ev_soc"].get<double>() * 100);
  ImGui::ProgressBar(float(s["soc"].get<double>()), {-1, 7}, "");
  check(host, c, "Batteria disponibile", "battery_available");
  slider(host, c, "Riserva SOC", "reserve", 0, .9f, "%.2f");
  check(host, c, "EV connessa elettricamente", "ev_connected");
  slider(host, c, "Limite wallbox", "ev_limit_w", 0, 11000, "%.0f W");
  if (!c["auto"].get<bool>()) {
    slider(host, c, "Setpoint batteria", "manual_battery_w", -3000, 3000, "%.0f W");
    slider(host, c, "Setpoint EV", "manual_ev_w", 0, 11000, "%.0f W");
  }
  ImGui::SeparatorText("Guasti e interruzioni");
  check(host, c, "Interrompi collegamento EMS", "drop_link");
  check(host, c, "Misure mancanti", "missing_measure");
  if (ImGui::Button("Nuova sessione / riconnessione"))
    host.submit({{"op", "reconnect"}});
  ImGui::TextWrapped("Fallback: batteria e EV a zero; i cicli protetti continuano. I carichi "
                     "prioritari possono superare il limite di rete: viene segnalato un allarme.");
}
void plant_panel(Host &host, const json &s, const std::filesystem::path &root) {
  heading("Impianto e risorse pubbliche",
          "Parametri del modello e confronto con rese storiche PVGIS");
  const auto &c = s["config"];
  if (ImGui::BeginTable("plant", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableNextColumn();
    ImGui::SeparatorText("Configurazione elettrica");
    int count = c["south_count"] == 10 ? 0 : 1;
    if (ImGui::Combo("Preset sud", &count,
                     "10 moduli / cablaggio 10\0"
                     "16 moduli / cablaggio 16\0"))
      patch(host, "south_count", count ? 16 : 10);
    check(host, c, "Gruppo nord opzionale / 16 moduli", "north");
    slider(host, c, "Potenza modulo", "module_wp", 100, 650, "%.0f Wp");
    slider(host, c, "Limite inverter AC", "inverter_w", 1000, 20000, "%.0f W");
    slider(host, c, "Limite prelievo rete", "grid_limit_w", 1000, 20000, "%.0f W");
    slider(host, c, "Capacita accumulo", "capacity_wh", 1000, 50000, "%.0f Wh");
    slider(host, c, "Limite carica", "charge_w", 0, 10000, "%.0f W");
    slider(host, c, "Limite scarica", "discharge_w", 0, 10000, "%.0f W");
    slider(host, c, "Obiettivo SOC EV", "ev_target", .1f, 1, "%.2f");
    ImGui::SeparatorText("Posizione e orientamento del modello");
    int city = std::abs(c["latitude"].get<double>() - 44.4949) < .01   ? 0
               : std::abs(c["latitude"].get<double>() - 38.1157) < .01 ? 2
                                                                       : 1;
    if (ImGui::Combo("Localita rapida", &city, "Bologna\0Roma\0Palermo\0")) {
      const double lat[]{44.4949, 41.9028, 38.1157}, lon[]{11.3426, 12.4964, 13.3615};
      host.submit(
          {{"op", "configure"}, {"patch", {{"latitude", lat[city]}, {"longitude", lon[city]}}}});
    }
    slider(host, c, "Latitudine", "latitude", 35, 48, "%.4f");
    slider(host, c, "Longitudine", "longitude", 6, 19, "%.4f");
    slider(host, c, "Inclinazione elettrica", "tilt_deg", 0, 60, "%.1f deg");
    slider(host, c, "Azimut (0 = sud)", "aspect_deg", -180, 180, "%.1f deg");
    ImGui::TextWrapped("Posizione e orientamento regolano il calcolo della produzione simulata.");
    ImGui::TableNextColumn();
    ImGui::SeparatorText("PVGIS / produzione storica");
    static int reference_city = 1;
    ImGui::Combo("Riferimento", &reference_city, "Bologna\0Roma\0Palermo\0");
    const char *names[]{"bologna", "roma", "palermo"};
    const auto sources = read_json(root / "SLX_SolarPilot/config/pvgis_sources.json");
    const auto &source = sources["items"][reference_city];
    const auto data = read_json(root / "SLX_SolarPilot" / source["file"].get<std::string>());
    double months[12]{}, yield[12]{}, annual = 0;
    const double kwp = c["south_count"].get<int>() * c["module_wp"].get<double>() / 1000;
    for (const auto &v : data["outputs"]["monthly"]["fixed"]) {
      int i = v["month"].get<int>() - 1;
      months[i] = i + 1;
      yield[i] = v["E_m"].get<double>() * kwp;
      annual += yield[i];
    }
    ImGui::TextColored(green, "Sud %.2f kWp | %.0f kWh/anno", kwp, annual);
    if (ImPlot::BeginPlot("Produzione mensile storica", {-1, 290})) {
      ImPlot::SetupAxes("Mese", "Energia [kWh]", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
      ImPlot::SetNextFillStyle(green);
      ImPlot::PlotBars("PVGIS sud", months, yield, 12, .65);
      ImPlot::EndPlot();
    }
    ImGui::TextWrapped("Riferimento: %s, 1 kWp, sud, inclinazione 26.565 gradi, SARAH3, loss=14 "
                       "gia incluso. Il grafico scala soltanto la potenza del gruppo sud; non "
                       "applica altre perdite. Non rappresenta una previsione meteo.",
                       names[reference_city]);
    ImGui::TextWrapped(
        "Il riferimento storico usa i parametri della località selezionata; rimane indipendente "
        "dalle regolazioni del modello simulato.");
    if (c["north"].get<bool>())
      ImGui::TextColored(orange, "Nord: escluso dal riferimento sud; simulato separatamente.");
    if (ImGui::TreeNode("Provenienza e parametri")) {
      const auto text = source.dump(2);
      ImGui::TextWrapped("%s", text.c_str());
      ImGui::TreePop();
    }
    ImGui::EndTable();
  }
}
void scenarios(Host &host, int scenario, const std::filesystem::path &root) {
  static std::future<json> pending;
  static json comparison = nullptr;
  static std::string comparison_error;
  heading("Scenari ripetibili",
          "Ogni prova riparte dalle stesse condizioni iniziali e apre una nuova sessione");
  ImGui::BeginDisabled(pending.valid());
  if (ImGui::Button("Confronta 3 policy / 2 ore simulate")) {
    comparison_error.clear();
    pending = std::async(std::launch::async, [scenario] { return compare_policies(scenario); });
  }
  ImGui::EndDisabled();
  if (pending.valid()) {
    ImGui::SameLine();
    ImGui::TextDisabled("Calcolo indipendente in corso...");
    if (pending.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      try {
        comparison = pending.get();
      } catch (const std::exception &e) {
        comparison_error = e.what();
      }
    }
  }
  if (!comparison_error.empty())
    ImGui::TextColored(red, "%s", comparison_error.c_str());
  if (!comparison.is_null()) {
    ImGui::TextDisabled("Stesse condizioni iniziali / scenario %d / 7200 passi per strategia",
                        comparison["scenario"].get<int>());
    if (ImGui::BeginTable("comparison", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      for (const char *h :
           {"Strategia", "Prelievo [kWh]", "Immissione [kWh]", "EV [kWh]", "SOC finale [%]"})
        ImGui::TableSetupColumn(h);
      ImGui::TableHeadersRow();
      for (const auto &r : comparison["results"]) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(r["policy"].get<std::string>().c_str());
        for (const char *k : {"import_wh", "export_wh", "ev_wh"}) {
          ImGui::TableNextColumn();
          ImGui::Text("%.3f", r["energy"][k].get<double>() / 1000);
        }
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", r["soc"].get<double>() * 100);
      }
      ImGui::EndTable();
    }
    ImGui::TextWrapped("Manuale: EV richiesta a 3.68 kW, accumulo a zero. Bilanciata: prima "
                       "accumulo. Pubblica: prima EV. L'energia EV servita puo differire: "
                       "confrontare anche EV e SOC, non soltanto il prelievo.");
    if (ImGui::Button("Salva confronto JSON")) {
      try {
        std::filesystem::create_directories(root / "output");
        std::ofstream f(root / "output/policy-comparison.json");
        if (!f)
          throw std::runtime_error("Impossibile salvare confronto");
        f << comparison.dump(2);
        comparison_error = "Salvato output/policy-comparison.json";
      } catch (const std::exception &e) {
        comparison_error = e.what();
      }
    }
  }
  const char *names[]{"Estate / surplus fotovoltaico",  "Inverno / bassa produzione",
                      "Nuvole rapide durante ricarica", "Rientro serale",
                      "Batteria vicina alla riserva",   "Sovraccarico domestico",
                      "Disconnessione e recupero EMS"};
  const char *descriptions[]{
      "Giugno, ore 11, 7.2 kWp, SOC 55%. Lavaggi richiesti all'EMS.",
      "Dicembre, copertura 75%, temperatura 7 C. Verificare minore produzione.",
      "Nuvolosita sinusoidale, periodo circa 565 s. Verificare la modulazione EV.",
      "Ore 19, luci 350 W e tutti i carichi attivi. Verificare il prelievo.",
      "SOC 15.1%, riserva 15%. La scarica si ferma alla riserva.",
      "Limite rete 3 kW, elettrodomestici attivi. Allarme senza spegnere cicli protetti.",
      "Collegamento interrotto fra 30 e 90 s simulati. EV e batteria a zero, poi recupero."};
  for (int i = 0; i < 7; ++i) {
    ImGui::PushID(i);
    ImGui::BeginChild("scenario", {0, 91}, ImGuiChildFlags_Borders);
    if (ImGui::Button("Carica", {90, 48}))
      host.submit({{"op", "reset"}, {"scenario", i}});
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextUnformatted(names[i]);
    ImGui::TextWrapped("%s", descriptions[i]);
    ImGui::EndGroup();
    ImGui::EndChild();
    ImGui::PopID();
  }
  ImGui::TextWrapped("Avviare il tempo dalla barra superiore. Pausa arresta i passi fisici; 'Passo "
                     "+1 s' scambia una misura e applica un solo passo. L'accelerazione cambia il "
                     "numero di passi eseguiti, non le loro dimensioni.");
}
void protocol(Host &host, const json &s) {
  heading("Monitor protocollo", "Comunicazione EEBUS e SunSpec / Modbus TCP in tempo reale");
  const auto &l = s.at("link");
  ImGui::Text("%s | ciclo %.3f ms | scambi %llu", l["endpoint"].get<std::string>().c_str(),
              l["rtt_ms"].get<double>(),
              static_cast<unsigned long long>(l["exchanges"].get<uint64_t>()));
  ImGui::TextWrapped("Messaggi EEBUS decifrati e telegrammi Modbus TCP completi. Seleziona una "
                     "riga per osservare contenuto, direzione e byte trasmessi.");
  static ImGuiTextFilter filter;
  filter.Draw("Filtro direzione / tipo", 300);
  static bool freeze = false;
  static bool follow = true;
  ImGui::SameLine();
  ImGui::Checkbox("Congela vista", &freeze);
  ImGui::SameLine();
  ImGui::Checkbox("Segui ultimo messaggio", &follow);
  static json frozen = json::array();
  if (!freeze)
    frozen = host.packets();
  static uint64_t selected = 0;
  static json detail = nullptr;
  if (ImGui::BeginTable("packettable", 6,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable,
                        {0, 270})) {
    for (const char *name : {"ID", "Direzione", "Tipo", "Sequenza", "Byte", "UTC epoch [ms]"})
      ImGui::TableSetupColumn(name);
    ImGui::TableHeadersRow();
    for (const auto &p : frozen) {
      const std::string text =
          p["direction"].get<std::string>() + " " + p["type"].get<std::string>();
      if (!filter.PassFilter(text.c_str()))
        continue;
      const uint64_t id = p["id"];
      ImGui::PushID(int(id));
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      if (ImGui::Selectable(std::to_string(id).c_str(), id == selected,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        selected = id;
        detail = p;
        follow = false;
      }
      ImGui::TableNextColumn();
      ImGui::TextColored(p["direction"].get<std::string>().find("TX") != std::string::npos ? accent
                                                                                           : green,
                         "%s", p["direction"].get<std::string>().c_str());
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(p["type"].get<std::string>().c_str());
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(p["seq"].get<uint64_t>()));
      ImGui::TableNextColumn();
      ImGui::Text("%d", p["bytes"].get<int>());
      ImGui::TableNextColumn();
      ImGui::Text("%llu", static_cast<unsigned long long>(p["real_ms"].get<uint64_t>()));
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  if ((detail.is_null() || follow) && !frozen.empty())
    detail = frozen.back();
  if (!detail.is_null()) {
    ImGui::SeparatorText("Dettaglio messaggio selezionato");
    if (ImGui::Button("Copia JSON"))
      ImGui::SetClipboardText(detail["payload"].dump(2).c_str());
    ImGui::SameLine();
    if (detail.value("truncated", false))
      ImGui::TextColored(orange, "Anteprima troncata a 16 KiB; byte totali nella tabella.");
    ImGui::TextDisabled("%s / %d byte", detail["direction"].get<std::string>().c_str(),
                        detail["bytes"].get<int>());
    if (ImGui::BeginTabBar("decode")) {
      if (ImGui::BeginTabItem("Payload decodificato")) {
        ImGui::BeginChild("json", {0, 0});
        const auto text = detail["payload"].dump(2);
        ImGui::TextUnformatted(text.c_str());
        ImGui::EndChild();
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Byte esadecimali")) {
        ImGui::BeginChild("hex", {0, 0});
        ImGui::TextWrapped("%s", detail["hex"].get<std::string>().c_str());
        ImGui::EndChild();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }
}
} // namespace
int application(int argc, char **argv) {
  std::filesystem::path root;
  int capture_frames = 0, initial_page = 0, initial_view = 0;
  bool demo = false, north = false, south10 = false, orbit_test = false;
  bool show_about = false, show_guide = false;
  std::string controller_peer;
  std::string capture;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "--version") {
      std::cout << "SLX SolarPilot " SLX_SOLARPILOT_VERSION "\n";
      return 0;
    } else if (a == "--about")
      show_about = true;
    else if (a == "--guide")
      show_guide = true;
    else if (a == "--root" && i + 1 < argc)
      root = argv[++i];
    else if (a == "--capture" && i + 1 < argc) {
      capture = argv[++i];
      capture_frames = 90;
    } else if (a == "--page" && i + 1 < argc)
      initial_page = std::clamp(std::stoi(argv[++i]), 0, page_count - 1);
    else if (a == "--view" && i + 1 < argc)
      initial_view = std::clamp(std::stoi(argv[++i]), 0, 5);
    else if (a == "--capture-frames" && i + 1 < argc)
      capture_frames = std::clamp(std::stoi(argv[++i]), 1, 3600);
    else if (a == "--demo")
      demo = true;
    else if (a == "--north")
      north = true;
    else if (a == "--south10")
      south10 = true;
    else if (a == "--peer" && i + 1 < argc)
      controller_peer = argv[++i];
    else if (a == "--orbit-test")
      orbit_test = true;
  }
  if (!capture.empty())
    capture = std::filesystem::absolute(capture).string();
  if (!controller_peer.empty())
    controller_peer = std::filesystem::absolute(controller_peer).string();
  root = application_root(root);
  std::filesystem::current_path(root);
  glfwSetErrorCallback([](int, const char *text) { std::cerr << text << '\n'; });
  if (!glfwInit())
    throw std::runtime_error("GLFW initialization failed");
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow *window =
      glfwCreateWindow(1640, 1000, "SLX SolarPilot | Silicon LogiX", nullptr, nullptr);
  if (!window)
    throw std::runtime_error("OpenGL 3.3 non disponibile");
  glfwSetWindowSizeLimits(window, 1150, 740, GLFW_DONT_CARE, GLFW_DONT_CARE);
  int iw = 0, ih = 0, ic = 0;
  auto *icon_pixels =
      stbi_load((root / "assets/branding/solarpilot.png").string().c_str(), &iw, &ih, &ic, 4);
  if (icon_pixels) {
    GLFWimage icon{iw, ih, icon_pixels};
    glfwSetWindowIcon(window, 1, &icon);
    stbi_image_free(icon_pixels);
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);
  gl::load();
  ImGui::CreateContext();
  ImPlot::CreateContext();
  auto &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.IniFilename = nullptr;
  const auto font = std::filesystem::path("C:/Windows/Fonts/segoeui.ttf");
  if (std::filesystem::exists(font))
    io.Fonts->AddFontFromFileTTF(font.string().c_str(), 16);
  theme();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330 core");
  int frame = 0, page = initial_page;
  bool playing = false, cutaway = false, showcase = false;
  float speed = 60, right_width = 335, bottom_height = 240, exposure = .95f;
  std::string selected, status;
  int selected_view = initial_view;
  cutaway = initial_view == 2 || initial_view == 3 || initial_view == 5;
  double clip_s = 0;
  bool replaying = false, replay_play = false;
  double replay_accumulator = 0;
  int replay_index = 0;
  std::vector<json> replay;
  char file_path[512] = "recordings/demo.slxrun";
  std::vector<Sample> replay_history;
  {
    Host host;
    const auto saved_network = root / "workspace/network-profile.json";
    if (std::filesystem::exists(saved_network)) {
      const auto network_profile = read_json(saved_network);
      if (network_profile.value("auto_start",false) && network_profile.value("role",0) == 0) {
        playing = true; speed = 1; host.playback(true,1);
      }
    }
    json mechanism_pose = json::object();
    const auto appliances = read_json(root / "SLX_SolarPilot/config/appliance_asset_contract.json");
    Renderer renderer(root / "SLX_SolarPilot");
    const auto brand = renderer.image_file(root / "assets/branding/company-logo.png");
    product_brand_image(brand);
    renderer.focus(initial_view);
    if (north || south10)
      host.submit(
          {{"op", "configure"}, {"patch", {{"north", north}, {"south_count", south10 ? 10 : 16}}}});
    if (!controller_peer.empty()) {
      host.submit({{"op", "eebus_start"}, {"role", "plant"}, {"port", 7447}});
      host.submit({{"op", "eebus_pair"}, {"peer", read_json(controller_peer)}});
    }
    if (demo) {
      playing = true;
      host.playback(true, speed);
    }
    auto last = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
      if (orbit_test)
        renderer.orbit(.9f, 0);
      const auto now = std::chrono::steady_clock::now();
      const double dt = std::chrono::duration<double>(now - last).count();
      last = now;
      if (showcase)
        clip_s += dt;
      if (replay_play && !replay.empty()) {
        replay_accumulator += dt * 30;
        const int frames = int(replay_accumulator);
        replay_accumulator -= frames;
        replay_index = std::min(int(replay.size() - 1), replay_index + frames);
      }
      auto state = host.state();
      if (replaying && !replay.empty())
        state = replay[replay_index];
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      if (ImGui::IsKeyPressed(ImGuiKey_F12))
        show_about = true;
      if (ImGui::IsKeyPressed(ImGuiKey_F1))
        show_guide = true;
      const auto *vp = ImGui::GetMainViewport();
      ImGui::SetNextWindowPos(vp->Pos);
      ImGui::SetNextWindowSize(vp->Size);
      ImGui::Begin("SolarPilot", nullptr,
                   ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                       ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
      const float sidebar = 196;
      ImGui::GetWindowDrawList()->AddRectFilled({vp->Pos.x, vp->Pos.y},
                                                {vp->Pos.x + sidebar + 18, vp->Pos.y + vp->Size.y},
                                                IM_COL32(17, 28, 49, 255));
      ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{.067f, .11f, .19f, 1});
      ImGui::BeginChild("Navigation", {sidebar, 0}, ImGuiChildFlags_None);
      ImGui::PushStyleColor(ImGuiCol_Text, {.91f, .94f, .99f, 1});
      ImGui::Image(static_cast<ImTextureID>(brand), {184, 103.5f});
      ImGui::Dummy({0, 4});
      ImGui::PushFont(nullptr, 27);
      ImGui::TextUnformatted("SolarPilot");
      ImGui::PopFont();
      ImGui::TextColored({.65f, .7f, .83f, 1}, "Simulatore energetico");
      ImGui::Dummy({0, 24});
      ImGui::PushStyleColor(ImGuiCol_Button, {.067f, .11f, .19f, 1});
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {.16f, .21f, .34f, 1});
      for (int i = 0; i < page_count; ++i) {
        const bool active = page == i;
        if (active)
          ImGui::PushStyleColor(ImGuiCol_Button, accent);
        if (ImGui::Button(pages[i], {sidebar - 12, 36}))
          page = i;
        if (active)
          ImGui::PopStyleColor();
      }
      ImGui::PopStyleColor(2);
      ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY() + 16, ImGui::GetWindowHeight() - 104));
      ImGui::TextColored({.65f, .7f, .83f, 1}, "PC SIMULATOR / v" SLX_SOLARPILOT_VERSION);
      ImGui::TextColored({.65f, .7f, .83f, 1}, "EEBUS | SunSpec");
      ImGui::TextColored({.65f, .7f, .83f, 1}, "Marco Pezzullo / 2026");
      ImGui::PopStyleColor();
      ImGui::EndChild();
      ImGui::PopStyleColor();
      ImGui::SameLine();
      ImGui::BeginChild("Workspace", {0, 0});
      if (state.is_null()) {
        ImGui::TextUnformatted("Avvio SLX SolarPilot...");
        const auto error = host.error();
        if (!error.empty())
          ImGui::TextColored(red, "%s", error.c_str());
      } else {
        ImGui::PushFont(nullptr, 24);
        ImGui::TextUnformatted(pages[page]);
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - 400));
        ImGui::TextColored(replaying ? orange : green, "%s",
                           replaying                                ? "RIPRODUZIONE REGISTRATA"
                           : state["link"].value("external", false) ? "PC IMPIANTO / EMS ESTERNO"
                                                                    : "PC SOFTWARE / EMS LOCALE");
        const bool fresh = state["link"]["fresh"];
        ImGui::TextColored(fresh ? green : orange, "%s",
                           fresh ? "Controllo attivo / comandi ricevuti"
                                 : "In attesa del controllore");
        ImGui::SameLine();
        ImGui::TextDisabled("%s", state["scenario_name"].get<std::string>().c_str());
        ImGui::BeginDisabled(replaying || showcase);
        if (ImGui::Button(playing ? "Pausa" : "Avvia", {80, 0})) {
          playing = !playing;
          host.playback(playing, speed);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(playing);
        if (ImGui::Button("Passo +1 s"))
          host.submit({{"op", "advance"}});
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120);
        if (state["link"].value("live", false))
          speed = 1;
        ImGui::BeginDisabled(state["link"].value("live", false));
        if (ImGui::SliderFloat("Velocita", &speed, 1, 3600, "%.0fx", ImGuiSliderFlags_Logarithmic))
          host.playback(playing, speed);
        ImGui::EndDisabled();
        if (state["link"].value("live", false)) {
          ImGui::SameLine();
          ImGui::TextDisabled("Rete live 1x");
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::Text("t = %.0f s | %02d:%02d", state["sim_s"].get<double>(),
                    int(state["hour"].get<double>()), int(state["hour"].get<double>() * 60) % 60);
        ImGui::Separator();
        const auto err = host.error();
        if (!err.empty())
          ImGui::TextColored(red, "%s", err.c_str());
        if (state["overload"].get<bool>())
          ImGui::TextColored(red, "LIMITE DI PRELIEVO SUPERATO / carichi prioritari preservati");
        if (page == 0) {
          const float width = (ImGui::GetContentRegionAvail().x - 40) / 5;
          metric("pv", "FOTOVOLTAICO", state["pv_w"].get<double>() / 1000, "kW", green, width);
          ImGui::SameLine();
          metric("home", "CONSUMO CASA", state["house_w"].get<double>() / 1000, "kW", orange,
                 width);
          ImGui::SameLine();
          metric("battery", "BATTERIA (+ scarica)", state["battery_w"].get<double>() / 1000, "kW",
                 accent, width);
          ImGui::SameLine();
          metric("ev", "RICARICA EV", state["ev_w"].get<double>() / 1000, "kW", green, width);
          ImGui::SameLine();
          metric("grid", "RETE (+ prelievo)", state["grid_w"].get<double>() / 1000, "kW",
                 state["grid_w"].get<double>() > 0 ? orange : green, width);
          auto size = ImGui::GetContentRegionAvail();
          right_width = std::clamp(right_width, 270.f, std::max(270.f, size.x - 450));
          bottom_height = std::clamp(bottom_height, 140.f, std::max(140.f, size.y - 240));
          const float left = size.x - right_width - 12;
          ImGui::BeginChild("Left", {left, 0});
          ImGui::BeginChild("Scene", {0, size.y - bottom_height - 9}, ImGuiChildFlags_Borders);
          ImGui::SetNextItemWidth(155);
          if (ImGui::Combo(
                  "Vista", &selected_view,
                  "Orbitale\0Tetto / fotovoltaico\0Locale tecnico\0Cucina\0Ricarica EV\0Scala\0")) {
            renderer.focus(selected_view);
            cutaway = selected_view == 2 || selected_view == 3 || selected_view == 5;
          }
          ImGui::SameLine();
          ImGui::Checkbox("Sezione", &cutaway);
          ImGui::SameLine();
          if (ImGui::Checkbox("Presentazione asset", &showcase)) {
            playing = false;
            host.playback(false, speed);
            clip_s = 0;
          }
          ImGui::SameLine();
          ImGui::SetNextItemWidth(100);
          if (ImGui::SliderFloat("Luce", &exposure, .45f, 1.6f, "%.2f"))
            renderer.exposure(exposure);
          ImGui::TextDisabled("Trascina destro: orbita | Rotella: zoom | Click: selezione");
          auto image_pos = ImGui::GetCursorScreenPos();
          auto image_size = ImGui::GetContentRegionAvail();
          image_size.x = std::max(64.f, image_size.x);
          image_size.y = std::max(64.f, image_size.y);
          const auto tex = renderer.render(int(image_size.x), int(image_size.y), state, cutaway,
                                           showcase, clip_s, mechanism_pose);
          ImGui::Image(static_cast<ImTextureID>(tex), image_size, {0, 1}, {1, 0});
          if (ImGui::IsItemHovered()) {
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
              renderer.orbit(io.MouseDelta.x, io.MouseDelta.y);
            if (io.MouseWheel)
              renderer.zoom(io.MouseWheel);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
              selected = renderer.pick((io.MousePos.x - image_pos.x) / image_size.x,
                                       (io.MousePos.y - image_pos.y) / image_size.y);
          }
          ImGui::EndChild();
          ImGui::InvisibleButton("horizontal_split", {left, 6});
          if (ImGui::IsItemActive())
            bottom_height -= io.MouseDelta.y;
          if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
          ImGui::BeginChild("Energy", {0, 0}, ImGuiChildFlags_Borders);
          ImGui::TextUnformatted("Energia e storico");
          ImGui::SameLine();
          ImGui::TextDisabled("PV %.2f kWh | Prelievo %.2f kWh | Bilancio %.3g W",
                              state["energy"]["pv_wh"].get<double>() / 1000,
                              state["energy"]["import_wh"].get<double>() / 1000,
                              state["balance_w"].get<double>());
          chart(replaying ? replay_history : host.history(), ImGui::GetContentRegionAvail().y);
          ImGui::EndChild();
          ImGui::EndChild();
          ImGui::SameLine(0, 2);
          ImGui::InvisibleButton("vertical_split", {6, size.y});
          if (ImGui::IsItemActive())
            right_width -= io.MouseDelta.x;
          if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
          ImGui::SameLine(0, 4);
          ImGui::BeginChild("Inspector", {0, 0}, ImGuiChildFlags_Borders);
          ImGui::BeginDisabled(replaying || showcase);
          controls(host, state, selected, mechanism_pose, appliances);
          ImGui::EndDisabled();
          ImGui::EndChild();
        } else if (page == 1) {
          ImGui::BeginChild("Loads");
          ImGui::BeginDisabled(replaying);
          loads_panel(host, state, selected);
          ImGui::EndDisabled();
          ImGui::EndChild();
        } else if (page == 2) {
          ImGui::BeginChild("Plant");
          ImGui::BeginDisabled(replaying);
          plant_panel(host, state, root);
          ImGui::EndDisabled();
          ImGui::EndChild();
        } else if (page == 3) {
          ImGui::BeginChild("Scenarios");
          ImGui::BeginDisabled(replaying);
          scenarios(host, state.at("scenario"), root);
          ImGui::EndDisabled();
          ImGui::EndChild();
        } else if (page == 4) {
          if (replaying)
            ImGui::TextWrapped(
                "Chiudere la riproduzione per consultare il traffico della sessione live.");
          else
            protocol(host, state);
        } else if (page == 5) {
          heading("Registrazioni e risultati",
                  "Snapshot confermati in JSON Lines, CSV e riproduzione locale");
          ImGui::SetNextItemWidth(-1);
          ImGui::InputText("##file", file_path, sizeof(file_path));
          ImGui::BeginDisabled(replaying);
          if (ImGui::Button("Registra sessione")) {
            try {
              auto path = root / file_path;
              if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path());
              host.submit({{"op", "record_start"}, {"path", path.string()}});
              status = "Registrazione richiesta";
            } catch (const std::exception &e) {
              status = e.what();
            }
          }
          ImGui::SameLine();
          if (ImGui::Button("Termina registrazione"))
            host.submit({{"op", "record_stop"}});
          ImGui::SameLine();
          if (ImGui::Button("Esporta CSV")) {
            try {
              std::filesystem::create_directories(root / "output");
              host.export_csv(root / "output/solarpilot-history.csv");
              status = "Salvato output/solarpilot-history.csv";
            } catch (const std::exception &e) {
              status = e.what();
            }
          }
          ImGui::EndDisabled();
          if (ImGui::Button("Apri riproduzione")) {
            try {
              auto loaded = read_recording(root / file_path);
              replay = std::move(loaded);
              replay_index = 0;
              replaying = true;
              replay_play = false;
              playing = false;
              host.playback(false, speed);
              replay_history.clear();
              for (const auto &v : replay)
                replay_history.push_back({v["sim_s"], v["pv_w"], v["house_w"], v["battery_w"],
                                          v["ev_w"], v["grid_w"], v["soc"]});
              status = "Riproduzione caricata: usare Panoramica per la scena";
            } catch (const std::exception &e) {
              status = e.what();
            }
          }
          if (replaying) {
            ImGui::SameLine();
            if (ImGui::Button("Chiudi riproduzione")) {
              replaying = false;
              replay_play = false;
            }
            ImGui::SameLine();
            if (ImGui::Button(replay_play ? "Pausa replay" : "Avvia replay"))
              replay_play = !replay_play;
            ImGui::SliderInt("Fotogramma registrato", &replay_index, 0, int(replay.size() - 1));
          }
          ImGui::TextWrapped("%s", status.c_str());
          ImGui::TextWrapped(
              "La riproduzione mostra i risultati registrati senza inviare comandi al modello. Il "
              "CSV esporta il ring dello storico live (massimo 4096 campioni); la registrazione "
              "conserva anche le richieste dell'operatore. Per prove lunghe usare il runner CLI.");
          ImGui::TextColored(state.value("recording", false) ? red : muted, "%s",
                             state.value("recording", false) ? "REGISTRAZIONE ATTIVA"
                                                             : "Registrazione inattiva");
          ImGui::TextDisabled("Replay automatico: 30 campioni/s reali.");
          ImGui::SeparatorText("Eventi della sessione");
          for (auto it = state["events"].rbegin(); it != state["events"].rend(); ++it)
            ImGui::Text("%8.0f s  %s", (*it)["t_s"].get<double>(),
                        (*it)["message"].get<std::string>().c_str());
        } else if (page == 7) {
          ImGui::BeginChild("EMS settings");
          ImGui::BeginDisabled(replaying);
          if (state["config"].contains("ev_mode"))
            ems_settings_panel(host, state);
          else
            ImGui::TextWrapped(
                "La registrazione precedente non contiene le nuove regolazioni EMS.");
          ImGui::EndDisabled();
          ImGui::EndChild();
        } else if (page == 8) {
          ImGui::BeginChild("Raspberry bench");
          ImGui::BeginDisabled(replaying);
          controller_bench_panel(host, state, root);
          ImGui::EndDisabled();
          ImGui::EndChild();
        } else if (page == 9) {
          ImGui::BeginChild("Customizations");
          product_customization_panel(root, state);
          ImGui::EndChild();
        } else {
          ImGui::BeginChild("About");
          product_about_panel(root, renderer.diagnostics());
          if (ImGui::Button("Apri finestra About / F12"))
            show_about = true;
          ImGui::SameLine();
          if (ImGui::Button("Guida rapida / F1"))
            show_guide = true;
          ImGui::EndChild();
        }
      }
      ImGui::EndChild();
      product_dialogs(root, renderer.diagnostics(), show_about, show_guide);
      ImGui::End();
      presentation_overlay(root,state,page,replaying);
      ImGui::Render();
      int fw, fh;
      glfwGetFramebufferSize(window, &fw, &fh);
      gl::Viewport(0, 0, fw, fh);
      gl::BindFramebuffer(GL_FRAMEBUFFER, 0);
      gl::ClearColor(.95f, .96f, .98f, 1);
      gl::Clear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      if (capture_frames && ++frame >= capture_frames &&
          (!demo || (!state.is_null() && state["link"].value("fresh", false)) ||
           ImGui::GetTime() > 15)) {
        renderer.screenshot(capture, fw, fh);
        std::ofstream report(capture + ".json");
        auto diagnostic = renderer.diagnostics();
        diagnostic["version"] = SLX_SOLARPILOT_VERSION;
        diagnostic["about_open"] = show_about;
        diagnostic["guide_open"] = show_guide;
        diagnostic["gl_error"] = gl::GetError();
        if (!state.is_null()) {
          diagnostic["protocol"] = state["link"]["protocol"];
          diagnostic["control_fresh"] = state["link"]["fresh"];
          diagnostic["control_exchanges"] = state["link"]["exchanges"];
          diagnostic["powers"] = {
              {"pv_w", state["pv_w"]},           {"house_w", state["house_w"]},
              {"battery_w", state["battery_w"]}, {"ev_w", state["ev_w"]},
              {"grid_w", state["grid_w"]},       {"balance_w", state["balance_w"]}};
        }
        report << diagnostic.dump(2);
        glfwSetWindowShouldClose(window, 1);
      }
      glfwSwapBuffers(window);
      if(glfwGetWindowAttrib(window,GLFW_VISIBLE)&&!glfwGetWindowAttrib(window,GLFW_ICONIFIED))
        presentation_frame_presented();
    }
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
} // namespace slx::desktop
int main(int argc, char **argv) {
  try {
    return slx::desktop::application(argc, argv);
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
#ifdef _WIN32
    MessageBoxA(nullptr, e.what(), "SLX SolarPilot", MB_OK | MB_ICONERROR);
#endif
    return 1;
  }
}
