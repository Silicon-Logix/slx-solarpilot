# EEBUS nel banco SolarPilot 0.4

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.



La GUI usa EEBUS SHIP 1.0.1 / SPINE 1.3.0 tramite lo stack MIT
[Enbility eebus-go](https://github.com/enbility/eebus-go), ship-go e spine-go.
Le revisioni e le somme dei moduli sono fissate in
`scripts/eebus.dependencies.lock.json` e `licenses/eebus/build-go.sum.txt`.

[SHIP](https://www.eebus.org/ship/) definisce la connessione sicura;
[SPINE](https://www.eebus.org/spine/) struttura dati e operazioni energetiche.
EEBUS è usato nel coordinamento energetico residenziale: per esempio
[SMA Sunny Home Manager](https://www.sma.de/produkte/energiemanagement/sunny-home-manager)
indica EEBUS e SEMP per l'integrazione dei carichi. Non tutti gli inverter usano
EEBUS: l'adattatore per un apparecchio dipenderà dai protocolli effettivamente
supportati, spesso SunSpec/Modbus. Le funzioni SunSpec implementate sono descritte in [INTEROPERABILITY.md](INTEROPERABILITY.md).

## Percorso effettivo

1. Il core pubblica potenze e SOC sul nodo impianto. Il modello genera i dati;
   SHIP trasporta SPINE via TLS/WebSocket, senza un broker.
2. Il nodo CEM riceve `measurementListData` attraverso Monitor Power Consumption
   (MPC). La policy C++ usa queste misure ricevute, non una copia diretta delle
   misure del modello. Configurazione e orari dell'EMS sono locali al controllore.
3. L'EMS invia `loadControlLimitListData` tramite Limitation of Power Consumption
   (LPC) e Limitation of Power Production (LPP).
4. Ogni scrittura attende il risultato SPINE correlato prima della successiva.
   In modalità locale si attendono anche i limiti effettivamente presenti nel nodo
   fisico, prima di applicare il passo. Nel banco esterno il nodo fisico controlla
   connessione e arrivo recente delle scritture.
5. Lo snapshot riporta le potenze applicate dal modello. Queste ritornano al CEM
   nel successivo aggiornamento MPC. Un `resultData.errorNumber=0` prova
   l'accettazione SPINE della scrittura, non l'esecuzione fisica dell'apparecchio.

Non viene incapsulato il vecchio JSON/CBOR proprietario dentro SPINE. Gli oggetti
JSON su stdin/stdout tra core e bridge sono IPC locale, non il protocollo di rete.
`link.ack` e gli ID di sessione dello SDK sono contabilità locale della simulazione,
non nuovi messaggi EEBUS o conferme standard di un prodotto commerciale.

## Profilo dimostrativo SLX

| Entità SPINE | Misure / controllo |
| --- | --- |
| 1: rete | Potenza W, positivo prelievo |
| 2: casa | Potenza W, consumo aggregato |
| 3: PV | Produzione W positiva; limite LPP |
| 4: batteria | Potenza W positiva scarica; SOC percentuale; LPC carica e LPP scarica |
| 5: EVSE | Potenza W, SOC percentuale; limite LPC |
| 6: lavatrice | Potenza W, DeviceDiagnosis; limite LPC |
| 7: lavastoviglie | Potenza W, DeviceDiagnosis; limite LPC |
| 8: PV disponibile | Stima W calcolata, prima del taglio inverter |

Il profilo fissa gli indirizzi entità sopra indicati, scoperti con il normale
`nodeManagementDetailedDiscoveryData`. Serve a identificare le parti dell'impianto
SLX e non è un profilo universale per apparecchi di altri produttori. Le misure
sono marcate come calcolate. I SOC SPINE sono percentuali, convertiti in 0..1 nello SDK.

Per i carichi `off` significa nessuna richiesta, `standby` richiesta in attesa,
`normalOperation` ciclo attivo. Il limite LPC autorizza l'avvio solo quando copre
la potenza massima della fase di riscaldamento. Un ciclo attivo resta protetto.
È una scelta del modello locale; non costituisce implementazione completa dei
casi d'uso di pianificazione degli elettrodomestici.

LPC/LPP trasmettono tetti non negativi. Il modello segue il tetto disponibile
entro i propri limiti e SOC. La differenza dei tetti di produzione e consumo
rappresenta la richiesta del modello batteria: non si dichiara il supporto
universale di un profilo di dispatch storage. Limiti del banco: 0..20 kW per entità,
durata scritta 2 h e failsafeDurationMinimum 2 h. La macchina di stato completa
di una centralina regolamentata non è qualificata da questa prova.

## Pairing e tempi

Due certificati ECDSA distinti sono creati localmente e riutilizzati ai riavvii.
Sono registrati SKI, fingerprint e SHIP ID del peer esplicitamente selezionato.
Il banco locale usa indirizzi loopback configurati. Il servizio stack apre il
listener SHIP; la fiducia è limitata ai peer registrati. Non si dichiara isolamento
del listener alla sola interfaccia loopback.

La 0.4 aggiunge mDNS LAN nativo, lettura identità da IP e associazione
esplicita, inventario e limiti generici. Vedere [INTEROPERABILITY.md](INTEROPERABILITY.md).
PIN e commissioning proprietari dipendono dal prodotto. Nessuna prova Raspberry
hardware è stata eseguita.

Misure MPC con timestamp più vecchio di 3 s vengono escluse dal controllore di
riferimento. Nel solo profilo SLX tutti e sei i limiti esterni devono essere rinnovati entro 3 s e SHIP connesso.
Perdendo il peer il passo fisico porta batteria ed EV a zero; cicli già avviati
continuano e il limite di immissione dell'inverter rimane operativo. è il watchdog
locale del dimostratore, distinto dai timer standard LPC/LPP. In pausa l'EMS
esterno può perdere validità delle misure: al riavvio della simulazione occorre
un nuovo scambio. Per questo EEBUS live usa 1x e passi di 1 s.

`drop_link` rimuove temporaneamente la registrazione fidata dei peer del banco
e chiude SHIP; rimuovendo il guasto
si registra nuovamente il peer. è un guasto software ripetibile, non una prova
di distacco fisico della rete. Un errore del bridge non attiva NNG come sostituto.

## Monitor

Il monitor osserva il corpo JSON SHIP dopo scrittura / prima del dispatch dello
stack. Non mostra header TCP, record TLS cifrati o discriminatore binario SHIP;
non è un PCAP. `PC TX`/`EMS RX` sono osservazioni del medesimo messaggio nei due
nodi, così come `EMS TX`/`PC RX`. In modalità esterna il PC vede solo il proprio nodo.

Contatori SPINE e `msgCounterReference` restano nei payload. La colonna sequenza
mostra `msgCounter` quando presente, zero per handshake senza contatore SPINE.
I tempi sono UTC epoch ms del PC. Ring di 256 osservazioni; anteprime massime
16 KiB esplicitamente marcate come troncate. Il conteggio byte è quello del corpo
osservato completo. JSON ed esadecimale rappresentano la stessa anteprima.

## Motore accelerato di laboratorio

CLI senza `eebus_start`, confronto policy e test energetici usano ancora il
trasporto interno NNG/TCP/CBOR. è una modalità separata per prove deterministiche
veloci e regressioni, non un protocollo solare. `config/protocol.schema.json`
descrive esclusivamente questa modalità legacy e la contabilità locale SDK.
L'interfaccia del banco EEBUS e il launcher dell'EMS di riferimento non usano NNG.

Questa release non è certificata EEBUS; non sono stati verificati apparecchi
commerciali, reti LAN fisiche o firmware ARM. La compatibilità delle singole
funzioni richiederà commissioning e test con i dispositivi di destinazione.

`link.rtt_ms` misura il ciclo completo del banco locale (pubblicazione, ricezione,
policy e arrivo dei limiti), non la sola latenza di un pacchetto. Durante discovery
può includere l'attesa iniziale. Nel banco esterno misura il servizio del passo fisico.
