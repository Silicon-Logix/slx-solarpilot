// Silicon LogiX / SLX SolarPilot
// Copyright (c) 2026 Marco Pezzullo. Author: Marco Pezzullo.
// License: Silicon LogiX Evaluation License 1.0. See LICENSE.
#include "product_panels.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <imgui.h>
#include <map>
#include <slx/version.hpp>
#include <sstream>
#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <shellapi.h>
// clang-format on
#endif
namespace slx::desktop {
namespace {
unsigned brand_texture = 0;

void heading(const char *title, const char *description) {
  ImGui::PushFont(nullptr, 25);
  ImGui::TextUnformatted(title);
  ImGui::PopFont();
  ImGui::TextWrapped("%s", description);
  ImGui::Spacing();
}
void open_link(const wchar_t *url) {
#ifdef _WIN32
  ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
#else
  (void)url;
#endif
}
std::string document(const std::filesystem::path &root, const char *name) {
  static std::map<std::filesystem::path, std::string> cache;
  const auto path = root / name;
  if (auto found = cache.find(path); found != cache.end())
    return found->second;
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return "Documento locale non disponibile: " + std::string(name);
  std::string text{std::istreambuf_iterator<char>(in), {}};
  if (text.size() > 512 * 1024)
    return "Documento oltre il limite di visualizzazione.";
  cache[path] = text;
  return text;
}
struct Extension {
  const char *id;
  const char *title;
  const char *detail;
};
constexpr Extension extensions[]{
    {"hmi-custom", "Interfacce e centraline personalizzate",
     "Pannelli touch, integrazione con impianti dedicati e configurazioni su misura."},
    {"device-adapters", "Inverter, contatori e wallbox reali",
     "Adattatori per i modelli selezionati, profili EEBUS e SunSpec/Modbus supportati dal "
     "costruttore, commissioning e prove di interoperabilita."},
    {"predictive-ems", "Ottimizzazione predittiva",
     "Previsioni meteo, tariffe dinamiche e pianificazione di accumulo e carichi con vincoli e "
     "criteri concordati."},
    {"hil", "Banco hardware e collaudo",
     "Misure strumentali, scenari di guasto, prove di temporizzazione e confronto fra simulatore e "
     "dispositivi."},
    {"reports", "Report e integrazioni dedicate",
     "Report aziendali, esportazioni personalizzate, integrazione con sistemi di supervisione e "
     "profili di impianto specifici."}};
} // namespace
void product_brand_image(unsigned texture) {
  brand_texture = texture;
}
void product_about_panel(const std::filesystem::path &root, const json &diagnostics) {
  if (brand_texture) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4{.067f, .11f, .19f, 1});
    ImGui::BeginChild("Company identity", {0, 112});
    ImGui::Image(static_cast<ImTextureID>(brand_texture), {178, 100});
    ImGui::EndChild();
    ImGui::PopStyleColor();
  }
  heading("SLX SolarPilot " SLX_SOLARPILOT_VERSION,
          "Simulazione e gestione energetica residenziale");
  ImGui::TextUnformatted("Autore: Marco Pezzullo  |  Copyright (c) 2026 Silicon LogiX");
  ImGui::TextDisabled("Windows x64 | C++20 | OpenGL 3.3 | EEBUS SHIP / SPINE | SunSpec Modbus TCP");
  ImGui::Spacing();
  if (ImGui::BeginTabBar("Product documents")) {
    if (ImGui::BeginTabItem("Prodotto")) {
      ImGui::SeparatorText("Funzionalità");
      ImGui::TextWrapped("Impianto 3D, profili di assorbimento, accumulo ed EV, priorita e "
                         "finestre dei carichi, limite export, monitor EEBUS, controllo EMS, "
                         "registrazioni, CSV e confronto delle policy.");
      ImGui::SeparatorText("Calcoli e campo di validita");
      ImGui::TextWrapped(
          "Potenze AC in W; energie in Wh; batteria positiva in scarica e rete positiva in "
          "prelievo. Accumulo: 95%% per direzione; EV: 60 kWh, rendimento 92%%. L'ultimo "
          "intervallo EV puo includere un arresto parziale al SOC obiettivo.");
      ImGui::TextWrapped("Il fotovoltaico e i profili degli apparecchi sono sintetici. Il limite "
                         "di rete genera un allarme quando i carichi non differibili lo superano. "
                         "La compatibilità dipende dai profili del dispositivo; il prodotto "
                         "non dispone di certificazione EEBUS.");
      ImGui::SeparatorText("Soluzioni su misura");
      ImGui::TextWrapped("La pagina Su richiesta presenta le integrazioni avanzate disponibili "
                         "tramite Silicon LogiX. F1 apre la guida; F12 apre le informazioni "
                         "sul prodotto da ogni pagina.");

      ImGui::EndTabItem();
    }
    for (const auto &item :
         {std::pair{"Licenza", "LICENSE"}, std::pair{"Terze parti", "THIRD_PARTY_NOTICES.md"},
          std::pair{"Interoperabilità", "docs/INTEROPERABILITY.md"},
          std::pair{"Modello", "docs/MODEL.md"}, std::pair{"Verifiche", "docs/VALIDATION.md"}}) {
      if (ImGui::BeginTabItem(item.first)) {
        ImGui::BeginChild(item.first, {0, 330}, ImGuiChildFlags_Borders);
        const auto text = document(root, item.second);
        ImGui::PushTextWrapPos(0);
        ImGui::TextUnformatted(text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndChild();
        ImGui::EndTabItem();
      }
    }
    if (ImGui::BeginTabItem("Diagnostica")) {
      ImGui::BeginChild("Diagnostic content", {0, 330}, ImGuiChildFlags_Borders);
      const auto text = diagnostics.dump(2);
      ImGui::TextUnformatted(text.c_str());
      ImGui::EndChild();
      if (ImGui::Button("Copia diagnostica"))
        ImGui::SetClipboardText(diagnostics.dump(2).c_str());
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::Spacing();
  if (ImGui::Button("Sito Silicon LogiX"))
    open_link(L"https://www.siliconlogix.it/");
  ImGui::SameLine();
  if (ImGui::Button("Contatti e integrazioni"))
    open_link(L"https://www.siliconlogix.it/contatti");
}
void product_customization_panel(const std::filesystem::path &root, const json &state) {
  heading("Funzionalita avanzate su richiesta",
          "Integrazioni e personalizzazioni da definire con Silicon LogiX");
  ImGui::TextWrapped(
      "Le estensioni seguenti sono disponibili su richiesta e richiedono un'attivazione dedicata.");
  static int selected = 0;
  for (int i = 0; i < int(std::size(extensions)); ++i) {
    const auto &item = extensions[i];
    ImGui::PushID(item.id);
    ImGui::Spacing();
    if (ImGui::RadioButton(item.title, selected == i))
      selected = i;
    ImGui::BeginDisabled();
    ImGui::Button("Attivazione su richiesta");
    ImGui::EndDisabled();
    ImGui::TextWrapped("%s", item.detail);
    ImGui::PopID();
  }
  ImGui::SeparatorText("Prepara una richiesta");
  static char notes[2048]{};
  ImGui::SetNextItemWidth(-1);
  ImGui::TextUnformatted("Note e obiettivo della prova");
  ImGui::InputTextMultiline("##request-notes", notes, sizeof(notes), {-1, 90});
  static std::string status;
  auto brief = [&]() {
    return json{{"schema", "slx.solarpilot.customization-request/1"},
                {"product", "SLX SolarPilot"},
                {"version", SLX_SOLARPILOT_VERSION},
                {"extension", extensions[selected].id},
                {"title", extensions[selected].title},
                {"notes", notes},
                {"config", state.at("config")},
                {"clock_hour", state.at("hour")},
                {"contact_url", "https://www.siliconlogix.it/contatti"}};
  };
  if (ImGui::Button("Salva scheda locale")) {
    try {
      const auto folder = root / "workspace/requests";
      std::filesystem::create_directories(folder);
      const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
      const auto path =
          folder / (std::string(extensions[selected].id) + "-" + std::to_string(stamp) + ".json");
      std::ofstream file(path);
      if (!file)
        throw std::runtime_error("Scrittura non riuscita");
      file << brief().dump(2);
      status = "Scheda salvata in workspace/requests/" + path.filename().string();
    } catch (const std::exception &e) {
      status = e.what();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Copia scheda"))
    ImGui::SetClipboardText(brief().dump(2).c_str());
  ImGui::SameLine();
  if (ImGui::Button("Apri pagina contatti"))
    open_link(L"https://www.siliconlogix.it/contatti");
  ImGui::TextWrapped("%s", status.c_str());
  ImGui::TextDisabled(
      "La scheda resta locale. L'apertura dei contatti non invia dati automaticamente.");
}
void product_dialogs(const std::filesystem::path &root, const json &diagnostics, bool &about,
                     bool &guide) {
  if (about)
    ImGui::OpenPopup("About SLX SolarPilot");
  const auto *viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowSize(
      {std::min(840.f, viewport->Size.x - 60), std::min(650.f, viewport->Size.y - 60)},
      ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("About SLX SolarPilot", &about, ImGuiWindowFlags_NoSavedSettings)) {
    product_about_panel(root, diagnostics);
    if (ImGui::Button("Chiudi")) {
      about = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (guide)
    ImGui::OpenPopup("Guida rapida SolarPilot");
  ImGui::SetNextWindowSize(
      {std::min(760.f, viewport->Size.x - 60), std::min(560.f, viewport->Size.y - 60)},
      ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("Guida rapida SolarPilot", &guide, ImGuiWindowFlags_NoSavedSettings)) {
    heading("La prima prova",
            "Il PC riproduce l'impianto; EEBUS trasporta misure e limiti fra impianto ed EMS.");
    ImGui::TextWrapped(
        "1. Carica uno scenario e premi Avvia. La comunicazione live procede in tempo reale.\n\n2. "
        "Confronta produzione, assorbimenti, ricarica e rete. Il segno positivo della rete indica "
        "prelievo; quello della batteria indica scarica.\n\n3. In Strategie EMS modifica EV, "
        "accumulo, finestre e limite export. In Monitor protocollo osserva i messaggi SHIP/SPINE "
        "effettivi.\n\n4. In Rete e dispositivi scegli EEBUS o SunSpec. Inserisci IP e porta "
        "del sistema oppure esponi l'impianto al controllore esterno. Per EEBUS verifica "
        "lo SKI e autorizza entrambi i dispositivi.\n\n5. Registra una sessione o esporta CSV. "
        "In Scenari confronta le strategie sulle stesse condizioni iniziali.");
    ImGui::Separator();
    ImGui::TextUnformatted(
        "Mouse destro: orbita | Rotella: zoom | Click: seleziona | F1: guida | F12: About");
    if (ImGui::Button("Chiudi guida")) {
      guide = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
} // namespace slx::desktop
