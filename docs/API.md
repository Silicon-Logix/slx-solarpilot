# SDK SLX SolarPilot — C ABI 1

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.



Header pubblico: `sdk/include/slx/solarpilot.h`. La DLL Windows x64 esporta
`slx_sp_abi_version`, `slx_sp_create`, `slx_sp_destroy`, `slx_sp_dispatch` e
`slx_sp_set_policy`. Calling convention C `__cdecl`; il numero ABI è 1.

Creare un contesto passando `SLX_SP_ABI_VERSION`; un numero incompatibile o un
errore di inizializzazione restituisce NULL. Distruggere il contesto con la stessa
DLL. Il chiamante serializza gli accessi; contesti diversi sono indipendenti.
La distruzione termina il thread EMS e chiude i socket.

`slx_sp_dispatch` riceve JSON UTF-8 di lunghezza esplicita, massimo 65536 byte.
Il chiamante fornisce un buffer di almeno `SLX_SP_RESPONSE_CAPACITY` (2 MiB).
Un buffer più piccolo produce `SLX_SP_BUFFER` prima di qualsiasi mutazione.
Il risultato è JSON UTF-8 terminato da NUL nel buffer del chiamante. Non occorre
liberare memoria della risposta. Gli errori sono convertiti in `{"error":"..."}`
e codice `SLX_SP_INVALID`; nessuna eccezione C++ attraversa il confine.

## Operazioni

| Richiesta | Risposta / effetto |
| --- | --- |
| `{"op":"snapshot"}` | Stato confermato, configurazione, energie, carichi, eventi e link |
| `{"op":"advance","steps":60}` | Esegue 60 passi da 1 s; ammessi 1–3600 per chiamata |
| `{"op":"reset","scenario":2}` | Azzera energie e tempo e crea una nuova sessione |
| `{"op":"configure","patch":{"cloud":0.8}}` | Valida atomicamente la configurazione e aggiorna i parametri |
| `{"op":"action","device":"washer","on":true}` | Richiede il ciclo; in automatico attende l'EMS |
| `{"op":"action","device":"oven","on":false}` | Spegne un carico interrompibile |
| `{"op":"action","device":"washer","available":false}` | Introduce indisponibilità, con arresto per guasto |
| `{"op":"action","device":"clock","hour":19.5}` | Modifica l'ora locale standard senza cambiare il tempo trascorso |
| `{"op":"action","device":"battery","soc":0.15}` | Imposta esplicitamente una condizione di prova |
| `{"op":"packets","after":100}` | Osservazioni successive all'ID 100, nel ring disponibile |
| `{"op":"reconnect"}` | Crea una nuova sessione applicativa e invalida vecchi comandi |
| `{"op":"last_command"}` | Ultimo comando accettato, per diagnostica |
| `{"op":"verify_command","command":{...}}` | Esegue i controlli di ammissibilità, senza azionare il mondo fisico |

`verify_command` è una diagnostica con stato: un comando accettato aggiorna la
sequenza vista. Per prove normali usare soltanto il percorso `advance`.

I dispositivi sono `fridge`, `washer`, `dishwasher`, `oven`, `cooktop`, `kettle`.
Il frigorifero è prioritario: si disabilita tramite `available`, non tramite `on`.
L'arresto operatore di lavatrice/lavastoviglie già attive è rifiutato. Il reset è
un riavvio dell'esperimento; l'indisponibilità è un guasto esplicito.

## Configurazione

Lo snapshot contiene l'intera configurazione e i valori iniziali. Il patch
respinge chiavi sconosciute, quantità di pannelli non cablate, valori fuori
intervallo e tipi incompatibili. Campi principali:

| Gruppo | Chiavi e unità |
| --- | --- |
| Fotovoltaico | `south_count` 10/16, `north` bool, `module_wp` Wp, `inverter_w` W |
| Posizione | `latitude`, `longitude`, `tilt_deg`, `aspect_deg` (0 sud, +90 ovest) |
| Ambiente | `month` 1–12, `cloud` 0–1, `temperature_c`, `rapid_clouds` bool |
| Casa | `base_w`, `lighting_w`, `grid_limit_w` |
| Batteria | `capacity_wh`, `reserve` 0–0.9, `charge_w`, `discharge_w`, `battery_available` |
| EV | `ev_connected`, `ev_limit_w`, `ev_target` 0.1–1 |
| Controllo | `auto`, `policy` balanced/custom, `manual_battery_w`, `manual_ev_w` |
| Guasti | `drop_link`, `missing_measure` |

Una modifica della capacità mantiene la frazione SOC; rappresenta quindi una
riconfigurazione con energia iniziale diversa. Per confronti dello stoccaggio
eseguire il reset fra configurazioni e non attribuire tale variazione a energia
trasferita durante la prova.

## Unità, segni e stato

Potenze in W, energie AC in Wh, tempo in secondi simulati, SOC in frazione 0–1,
temperatura in °C. `grid_w > 0` è prelievo; `battery_w > 0` è scarica.
`pv_w + battery_w + grid_w = house_w + ev_w` a ogni passo.
Le potenze nello snapshot dopo `advance` sono quelle dell'ultimo intervallo
integrato; lo stato dei cicli e i SOC sono quelli raggiunti al suo termine.

Lo snapshot indica `schema=slx.solarpilot.state/1`. La qualità di comunicazione è
`link.fresh`; il monitor conserva il payload con qualità `good` o `missing`.
Un dato di potenza simulata resta valido come stato del mondo anche se l'EMS non
ha risposto: il suo comando viene sostituito dalla policy di fallback.

## Policy pubblica

Registrare `slx_sp_policy_fn` e selezionare `policy=custom`. La funzione riceve
`slx_sp_policy_input` e scrive `slx_sp_policy_output` con versione e dimensione
esatte. Il callback è chiamato dal thread EMS: non deve bloccare, lanciare
eccezioni o richiamare il contesto. Puntatori validi soltanto durante la chiamata;
`user_data` resta del chiamante fino a deregistrazione o distruzione.
Le potenze restituite sono richieste, soggette ai limiti fisici del core.


## Estensioni 0.2: EEBUS e strategie EMS

La ABI resta 1. La GUI inizializza `eebus_start` con `role=pair` (due nodi nello
stesso bridge), `plant` (impianto che attende EMS esterno), oppure `controller`
(CEM senza clock fisico). `port` è opzionale, zero assegna una porta libera;
`state_dir` conserva le identità private. Non copiare questa cartella nelle release.

- `eebus_pair`, `peer={host,port,ski,fingerprint,ship_id}`: registra un'identità
  esplicita. I metadati pubblici sono salvati in `<state_dir>/*-peer.json`.
- `eebus_status`: stato SHIP e numero entità SLX scoperte.
- `eebus_tick`, `sim_s`: passo del solo EMS esterno. Configurazione, orario di
  base e scadenza EV sono del controllore, non un payload proprietario su EEBUS.
- `packets`: osservazioni SHIP/SPINE quando il bridge è attivo. `capture` precisa
  il punto di osservazione; `truncated` indica l'anteprima ridotta a 16 KiB.

`link.identity`, `role`, `live`, `error` completano lo stato; `link.fresh` indica
che il passo fisico usa limiti ricevuti validi. `link.ack` è l'esito locale del
modello, non una conferma SPINE dell'apparecchio. Il tempo live EEBUS è 1x; batch
accelerati restano disponibili solo per esperimenti offline/di laboratorio.
Un mancato avvio del bridge lascia l'impianto in fallback e non cambia trasporto.

Nuovi campi di configurazione (tutti inclusi nello snapshot):

| Gruppo | Chiavi |
| --- | --- |
| Rete | `export_limit_enabled`, `export_limit_w`, `grid_setpoint_w` |
| EV | `ev_mode` surplus/fast/departure, `ev_departure_hour`, `ev_quantize`, `ev_start_margin_w`, `ev_stop_delay_s` |
| Accumulo | `battery_schedule_enabled`, `battery_schedule_start_h`, `battery_schedule_end_h`, `battery_schedule_target` |
| Cicli | `washer_window_enabled`, `washer_start_h`, `washer_end_h`, `washer_priority`; analoghi `dishwasher_*` |
| Tariffe di prova | `tariff_enabled`, `tariff_import_eur_kwh`, `tariff_export_eur_kwh` |

Le finestre usano ore [0,24), supportano il passaggio di mezzanotte; inizio uguale
alla fine significa intera giornata. Un numero di priorità maggiore prevale.
Le nuove energie e i risultati economici sono descritti in [EMS.md](EMS.md).

Il trasporto storico NNG resta accessibile a CLI/test (`connect_controller`,
`listen_controller`) per regressioni di laboratorio. Non è l'EEBUS del banco.
Il callback pubblico usa il thread EMS NNG nei test, il worker che chiama lo SDK
nel percorso EEBUS; in entrambi i casi rimangono vietati blocchi e rientranza.


## Estensioni 0.3: potenza media e azioni

`ev_w` rimane la potenza media AC nell'ultimo passo di integrazione. I nuovi
`ev_setpoint_w` e `ev_active_s` indicano corrente/potenza ammissibile e durata
attiva dell'ultimo intervallo: quando il SOC obiettivo viene raggiunto nel passo,
la media può essere inferiore a 1380 W. Il ledger aggiunge `ev_stored_wh` e
`ev_loss_wh`. I segni di rete e batteria e la ABI 1 rimangono invariati.

`action` con `on=true` su un carico già avviato conserva il ciclo. Richieste con
più campi sono atomiche. Il frigorifero viene comandato con `available`; il
termostato governa il compressore e `on` viene respinto per evitare un falso
ciclo temporizzato. Le priorità di lavatrice/lavastoviglie richiedono interi.

## Rete e interoperabilit? 0.4

`eebus_start` aggiunge `lan`, `profile=standard|slx` e
`device=residential|pv-inverter|battery|evse`. L'operazione
`network_request` riceve `request` e restituisce sempre lo snapshot completo:
le risposte sono in `network[op]`, l'ultima lettura SunSpec in `network.sunspec_latest`.
Esempio: `{"op":"network_request","request":{"op":"sunspec_scan","host":"192.168.1.20","port":502,"unit":1}}`.
Contratto del servizio riutilizzabile su ARM: [ENERGY_SERVICE.md](ENERGY_SERVICE.md).
Mappe standard e propriet? SLX separate: [INTEROPERABILITY.md](INTEROPERABILITY.md).
