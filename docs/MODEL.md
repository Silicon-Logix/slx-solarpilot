# Modello energetico e scenari

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.



Il modello è deterministico con passo fisso di 1 s. Non usa numeri casuali per
misure, meteo o carichi. Solo l'identità delle sessioni è casuale.

## Produzione e bilancio

Posizione solare approssimata da latitudine, mese, ora standard e longitudine,
con meridiano di riferimento 15° E. Non sono applicati DST o equazione del tempo;
il giorno rappresentativo è il 21° del mese approssimato a 30 giorni. La nuvolosità
e la temperatura sono sintetiche. La componente diretta è proiettata sul piano
dei pannelli; quella diffusa dipende dall'altezza solare. Il gruppo nord viene
calcolato con azimut opposto al gruppo sud. Di notte la potenza PV è zero.

La potenza nominale è numero moduli × Wp, con fattore termico semplificato e
clipping AC dell'inverter. Modificare i pannelli non modifica automaticamente
inverter o capacità batteria. È un modello dimostrativo, privo di ombreggiamenti
locali, MPPT dettagliato e rete elettrica trifase.

Convenzioni: batteria positiva in scarica, rete positiva in prelievo. La rete
copre il residuo di potenza. Il limite domestico è una soglia di gestione e
allarme: non è un interruttore ideale che elimina energia. Cicli già attivi e
carichi prioritari possono superarlo. Lo snapshot espone il residuo del bilancio.

Le energie AC sono integrate da W Ã— s / 3600. Sono separati PV, casa, EV,
prelievo, immissione, carica e scarica della batteria. Il bilancio cumulativo è:

`PV + prelievo + scarica = casa + EV + immissione + carica`.

## Accumulo ed EV

Accumulo iniziale 10 kWh, SOC 55%, riserva 15%, limiti 3 kW per direzione.
Rendimento 95% in carica e in scarica. Ogni passo limita la potenza anche in
funzione dell'energia effettivamente disponibile prima del SOC limite.
La perdita di conversione è contabilizzata separatamente in Wh.

EV da 60 kWh, SOC iniziale 45%, obiettivo 80%, rendimento 92%. Potenza minima
dimostrativa 1380 W; sotto tale richiesta la ricarica è spenta. La potenza è
limitata da wallbox, collegamento e obiettivo SOC. Il minimo riguarda la
potenza attiva durante la ricarica: nell'ultimo passo il veicolo puÃ² fermarsi
prima di un secondo. ev_w è la media del passo; ev_setpoint_w ed ev_active_s
riportano la potenza attiva e la durata, evitando energia oltre il target. Non è simulato il protocollo
elettrico IEC 61851 o un veicolo commerciale.

In modalità automatica bilanciata il surplus carica prima la batteria fino al
95%, poi alimenta l'EV. Con deficit, la batteria puÃ² alimentare la casa fino alla
riserva. La policy pubblica mostra una diversa priorità EV. In manuale valgono
i setpoint operatore, sempre soggetti ai limiti del mondo fisico.

## Elettrodomestici

| Dispositivo | Profilo dimostrativo |
| --- | --- |
| Frigorifero | Modello termico del primo ordine, termostato 3–5 °C, compressore 125 W, standby 2 W |
| Lavatrice | 3600 s: acqua 80 W, riscaldamento 2200 W, lavaggio 160 W, risciacquo 100 W, centrifuga 500 W |
| Lavastoviglie | 5400 s: acqua 50 W, riscaldamento 2100 W, lavaggio 130 W, risciacquo caldo 1600 W, asciugatura 25 W |
| Forno | Preriscaldamento 2400 W per 600 s, poi duty cycle 55/120 s, durata massima 3600 s |
| Piano cottura | 1800 W per 1800 s oppure spegnimento operatore |
| Bollitore | 2000 W per 180 s con spegnimento automatico |

Lavatrice e lavastoviglie sono differibili prima dell'avvio e protette durante
il ciclo. L'EMS considera la potenza di riscaldamento per autorizzarne l'avvio,
senza promettere una previsione dei carichi futuri. L'indisponibilità esplicita
simula un guasto e puÃ² arrestare anche un ciclo protetto. L'ispezione geometrica
degli sportelli non è un modello termico della porta aperta.

## Scenari

| ID | Condizioni iniziali | Esito osservabile |
| --- | --- | --- |
| 0 | Giugno, 11:00, nuvole 10%, SOC 55%, lavaggi richiesti | Surplus e completamento dei cicli |
| 1 | Dicembre, 11:00, nuvole 75%, 7 °C | Produzione ridotta |
| 2 | Giugno, EV connessa, nuvole variabili deterministicamente | Modulazione con copertura a periodo circa 565 s |
| 3 | 19:00, luci 350 W, tutti i carichi avviati | Prelievo serale e possibile sovraccarico |
| 4 | SOC 15.1%, riserva 15% | Nessuna scarica sotto riserva |
| 5 | Limite rete 3 kW, carichi attivi | Allarme, cicli protetti conservati |
| 6 | Interruzione fra 30 e 90 s simulati | Fallback e ripresa da 90 s |

Per confrontare manuale/automatico o policy, resettare lo stesso scenario e usare
uguali parametri e durata. Il confronto GUI esegue tre istanze indipendenti del core sullo scenario
selezionato per 7200 passi ciascuna, senza modificare la prova live. In manuale
la richiesta EV e 3680 W e la batteria e a zero; le altre due prove usano le
rispettive policy. Vengono mostrati anche energia EV servita e SOC finale, per
evitarne l'interpretazione come un confronto a identico servizio erogato.

Registrazioni e CSV riportano risultati del modello;
non sono misure di un'abitazione reale o risultati economici.

## Dati PVGIS

Le tre risposte originali per Bologna, Roma e Palermo sono rese storiche
modellate per 1 kWp sud, inclinazione 26.565051°, SARAH3, `loss=14` già incluso.
La vista mensile le scala soltanto per i kWp del gruppo sud. Restano riferimenti
alla località e ai parametri della risposta selezionata: non seguono variazioni
arbitrarie dei parametri elettrici e non stimano la falda nord.

Il modello istantaneo è indipendente da quelle rese: non trasforma medie mensili
in una falsa previsione a secondi e non applica nuovamente il 14% ai dati PVGIS.
Nessuna funzione di previsione operativa legge il futuro della simulazione.

`scripts/pvgis.py --lat 41.9028 --lon 12.4964 --refresh` acquisisce un nuovo
riferimento in `workspace/pvgis/`, conservando parametri, URL, data UTC e SHA256.
`--offline` richiede una cache valida. Il mancato aggiornamento mantiene la cache
precedente. Il ciclo EMS non dipende da Internet. L'importazione automatica di
questi riferimenti personalizzati nel grafico GUI non è disponibile;
la GUI corrente usa esplicitamente i tre riferimenti inclusi.

Fonte primaria: [API PVGIS del JRC](https://joint-research-centre.ec.europa.eu/photovoltaic-geographical-information-system-pvgis/using-pvgis-5/api-non-interactive-service_en).


## Strategie di gestione

Vedere [EMS.md](EMS.md) per finestre, priorità, ricarica programmata e controllo
EV. Il limite export applicato dall'inverter rimane attivo anche senza EMS.
Le tariffe ipotetiche introducono risultati economici illustrativi, disabilitati
per default; non trasformano il modello in una previsione o in una bolletta.
Le prove energetiche accelerate usano il motore locale; il collegamento EEBUS
live scambia misure e limiti a 1x con lo stesso modello fisico.


## Contabilità della ricarica EV

Le energie ev_stored_wh ed ev_loss_wh separano il 92% immagazzinato dal
residuo dissipato: ev_wh = ev_stored_wh + ev_loss_wh. La scheda Strategie EMS
mostra anche questi ledger e il residuo di potenza. Registrazioni precedenti
senza tali campi riportano n/d per queste grandezze.
Vedere [verifiche numeriche](VALIDATION.md) per gli integrali attesi,
le correzioni dei casi limite e il campo di validità del modello.
