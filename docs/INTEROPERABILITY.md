# Interoperabilità SolarPilot 0.4

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.



## Matrice delle funzioni

| Percorso | Funzioni operative | Verifica eseguita |
| --- | --- | --- |
| EEBUS / SHIP | TLS, WebSocket, identità persistenti, mDNS LAN, associazione manuale da IP | Due processi Windows; peer mDNS osservato; SKI/fingerprint da TLS |
| EEBUS / SPINE | Inventario generico di entità e feature, MPC W/SOC, LPC/LPP con risultato correlato | Profilo inverter ridotto a due entità, indirizzo diverso dal banco SLX, singolo limite LPP applicato |
| SunSpec server | Modelli 1, 101, 120, 123; 1, 201; 1, 101, 120, 124 | Lettura e scrittura con pysunspec2 della SunSpec Alliance, implementazione client indipendente |
| SunSpec client | Scoperta catena, basi 40000/0/50000, modelli 1, 101–103, 111–113, 120, 123/124, 201–203, 211–213 | Controllo server separato; interi con scale e modelli float/trifase verificati da fixture TCP |
| SLX Energy/1 | EV e consensi lavatrice/lavastoviglie, watchdog | Banca registri separata, setpoint EV applicato e scadenza verificata |
| Servizio ARM | Stesso sorgente protocolli, target Linux ARMv7 e ARM64 senza CGO | Crosscompilazione e intestazioni ELF; esecuzione su Raspberry da qualificare |

I test EEBUS usano lo stack Enbility su entrambi i lati; verificano l'assenza del
vincolo alle otto entità SLX, senza sostituire un test con un altro costruttore.
Non sono dichiarate certificazioni o compatibilità universale. Non basta che
un apparecchio risponda a un IP: occorrono protocollo, ruolo, funzioni supportate
e permessi di scrittura. Le specifiche proprietarie del costruttore richiedono
un adattatore dedicato, disponibile su richiesta.

## EEBUS

Nella GUI scegliere impianto per EMS esterno oppure EMS per dispositivo esterno.
Impostare porta locale, profilo residenziale/inverter/accumulo/wallbox e LAN.
La ricerca usa `_ship._tcp` con backend Go nativo, senza Bonjour obbligatorio.
Il peer selezionato conserva il percorso WebSocket pubblicato via mDNS.
In alternativa leggere da IP/porta lo SKI e il fingerprint del certificato TLS.
La lettura non concede fiducia: confrontare lo SKI sul display del peer e premere
Associa. Anche l'altro sistema deve autorizzare SolarPilot. Le chiavi restano in
`workspace/eebus`, escluse dalla distribuzione.

Il ruolo EMS mostra le entità effettivamente dichiarate: identifica i dispositivi
per SKI e indirizzo SPINE; non richiede nomi, produttore o ordine SLX. Leggi
dispositivi aggiorna misure e capacità. Un limite vale per l'entità selezionata,
consumo LPC o produzione LPP, in W e con durata. Un risultato positivo significa
accettazione SPINE; le potenze nel simulatore mostrano l'esecuzione fisica.
Le altre entità non devono ricevere comandi per rendere valido un singolo limite.

Il banco SLX locale mantiene la policy completa a sei canali e il watchdog di
3 s. Il profilo standard usa i limiti delle singole entità e le relative durate;
non impone il watchdog a sei canali a un EMS generico. Nel modello, limiti di
consumo e produzione dell'accumulo delimitano un intervallo; la batteria segue
il bilancio domestico entro tale intervallo, senza interpretare due tetti
positivi come un setpoint obbligatorio. Senza controllo valido EV e batteria
si arrestano; i cicli già avviati e l'interblocco locale export restano operativi.
La macchina di stato regolamentata completa e i profili ulteriori non sono
qualificati da questo simulatore.

## SunSpec server

Tab SunSpec: IP locale `0.0.0.0` per LAN, porta predefinita **1502** configurabile
(502 se richiesta dal controllore). Il bind deve essere disponibile sul PC.
Non viene modificato il firewall Windows. Modbus TCP non autentica i comandi:
il listener va usato sulla LAN di laboratorio controllata dall'operatore.
Premere Esponi impianto, poi Avvia per far evolvere il modello a 1x.
Questo seleziona SunSpec come sorgente dei comandi; Arresta SunSpec ripristina
il percorso EEBUS. Usare il ruolo impianto per il banco SunSpec esterno.

Indirizzi di protocollo **zero-based**: firma `SunS` a 40000, primo modello a
40002; alcuni software presentano la stessa locazione come 40001. Scoprire
sempre ID/L della catena; gli indirizzi dei singoli modelli non sono fissi.
MBAP big-endian, Unit ID 1–247; FC03/04, FC06 e FC16, eccezioni Modbus per
indirizzi, conteggi e valori invalidi. Le scritture multiple sono atomiche.

| Unità | Catena | Grandezze e comandi implementati |
| --- | --- | --- |
| 1 / PV | 1, 101, 120, 123 | W, A, V, Hz, VA, PF, Wh, potenza nominale; Conn, WMaxLimPct, WMaxLim_Ena, WMaxLimPct_RvrtTms |
| 2 / rete | 1, 201 | W e Wh import/export; W positivo prelievo, negativo immissione |
| 3 / accumulo | 1, 101, 120, 124 | W positivo scarica, SOC %, Wh erogati, potenza nominale; StorCtl_Mod, InWRte, OutWRte, MinRsvPct, InOutWRte_RvrtTms, ChaGriSet |

I punti non simulati usano i sentinel SunSpec; non sono misure inventate di un
sensore. Tensione 230 V, frequenza 50 Hz, PF 100% e potenza reattiva zero sono
ipotesi monofase esplicite. Le scale sono lette dai registri, incluse quelle dei
controlli percentuali. Il contatore usa W_SF=1 (risoluzione 10 W) per evitare
overflow signed16 nei casi di carico elevato. I limiti fisici restano esatti
nel core; la quantizzazione riguarda soltanto la rappresentazione Modbus.

Per limitare PV: scrivere WMaxLimPct in percentuale e WMaxLim_Ena=1.
Conn=0 disabilita la produzione simulata. Un RvrtTms positivo ripristina il
controllo locale dopo quel numero di secondi reali; zero mantiene il comando.
I timer di rampa/finestra e controlli reattivi non sono implementati/scrivibili.

StorCtl_Mod bit 0 abilita il tetto di carica InWRte, bit 1 il tetto di scarica
OutWRte. Le percentuali si riferiscono a WChaMax e possono essere negative:
intervallo W = [-InWRte × WChaMax/100, OutWRte × WChaMax/100].
Con InWRte=50, OutWRte=-50 e StorCtl_Mod=3 si forza carica al 50%; l'inverso
forza scarica. Un intervallo impossibile viene rifiutato senza modifiche.
La riserva del modello, i limiti di potenza e il SOC restano vincoli fisici.
ChaGriSet=0 vieta carica da rete; 1 la consente nei limiti dell'impianto.

## Estensione facoltativa SLX Energy/1

Unità **10**, indirizzi **60000–60007**, fuori dalle catene SunSpec:

| Offset | Tipo / funzione |
| --- | --- |
| 0–2 | Sola lettura: 0x534c, 0x5831, versione 1 |
| 3 | Potenza EV, uint16 W, 0–11000; il minimo fisico resta 1380 W |
| 4–5 | Consenso avvio lavatrice/lavastoviglie, 0/1; cicli già avviati protetti |
| 6 | Abilita estensione, 0/1 |
| 7 | Watchdog secondi reali, 0 persistente oppure 1–3600 |

FC03/04/06/16. La scadenza annulla EV e nuovi consensi. Nessun campo SLX è
necessario per leggere o comandare PV e accumulo SunSpec standard.

## Servizio comune e verifiche ripetibili

`core/linux-armv7/slx-energy-service` e `core/linux-arm64/slx-energy-service`
sono lo stesso servizio di `slx-eebus.exe`. Il protocollo IPC è locale
stdin/stdout JSON Lines, non uno standard di rete. Il futuro firmware può
riusare questo servizio e mantenere la propria HMI e policy. Un esempio di
client pubblico e commentato è in `examples/energy_service_client.py`.
`docs/ENERGY_SERVICE.md` documenta il contratto. Non è un'immagine SD completa.

```powershell
ctest --test-dir build/ucrt --output-on-failure
python scripts/bootstrap_sunspec_tests.py
python tests/test_network.py build/ucrt/bin/slx-solarpilot-cli.exe --pysunspec build/interop-research/pysunspec2/pysunspec2-bbe9e044c2ae06af54b49e5ef042493eddb7e2b8
```

Il secondo comando verifica SHA256 del client Alliance e delle definizioni
standard fissate. Solo questi test opzionali richiedono Python con pyserial;
l'applicazione Windows e il servizio ARM sono binari autonomi.

Fonti: [EEBUS SHIP](https://www.eebus.org/ship/),
[EEBUS SPINE](https://www.eebus.org/spine/),
[definizioni SunSpec](https://github.com/sunspec/models),
[client SunSpec Alliance](https://github.com/sunspec/pysunspec2).
