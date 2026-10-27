# Servizio protocolli condiviso / 0.4

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.


Il servizio Windows slx-eebus.exe gestisce le comunicazioni EEBUS e SunSpec.
Espone un contratto locale JSON Lines per le applicazioni e gli strumenti SDK.

## Contratto IPC pubblico

Avvio: `--role plant|controller|pair --state-dir <cartella> --port <porta SHIP>`.
`--lan` attiva mDNS; `--profile standard|slx` e `--device residential|pv-inverter|battery|evse`
selezionano le entità del ruolo impianto. Solo il banco locale completo usa pair/slx.
I certificati persistono nella cartella di stato. Un processo figlio viene posseduto
dall'applicazione; EOF su stdin chiude il servizio. stdout contiene solo JSON Lines;
il primo oggetto espone le identità locali. Richieste serializzate, una risposta per
riga. Gli errori hanno campo `error`. Questo contratto locale non viaggia in rete.

| Operazione | Campi / risultato |
| --- | --- |
| network_status | peers mDNS, dispositivi, listener SunSpec |
| probe | host IP, port; restituisce SKI/fingerprint senza concedere fiducia |
| pair | peer: host, port, ski, fingerprint opzionale, ship_id opzionale; registrazione fidata esplicita |
| devices | entità, key SKI/indirizzo, tipo, feature e ultime misure |
| read_device | entity=key; richiede misure e restituisce cache, ripetere dopo risposta remota |
| write_limit | entity, direction consume/produce, value_w, duration_s; attende result SPINE correlato |
| sunspec_scan | host, port, unit, base opzionale (-1=automatico); catena e punti scalati |
| sunspec_write | stessi campi, model, point, value in unità fisiche; scrive solo scalari RW noti |
| sunspec_start | address IP:porta; avvia server dell'impianto virtuale |
| sunspec_stop | arresta il server |
| publish / meter | values numerici, config numerica, energy Wh, states; aggiorna l'impianto virtuale |
| sunspec_command | restituisce i comandi validati da applicare al modello fisico |
| applied | comandi/limiti EEBUS ricevuti e validità |
| packets | after ID; fino a 16 osservazioni per risposta, ring 256; richiedere il lotto successivo |
| shutdown | termina ordinatamente |

Il ruolo controller può leggere/comandare peer EEBUS e SunSpec reali; non richiede
la DLL fisica Windows. I ruoli plant/server necessitano dei valori pubblicati
dall'applicazione che simula o misura l'impianto. `examples/energy_service_client.py`
mostra lettura SunSpec e ispezione/associazione EEBUS senza implementare protocolli.
## Uso dal core Windows

`eebus_start` accetta anche `lan`, `profile`, `device`, `port`.
`network_request` inoltra una richiesta IPC nel campo `request` e restituisce
uno snapshot completo con `network`, conservando il contratto Host/SDK ABI 1.
`sunspec_start` seleziona i comandi SunSpec per il modello; `sunspec_stop` ripristina
EEBUS. Il monitor resta unico. Per le unità, scale e funzioni effettivamente
supportate vedere [INTEROPERABILITY.md](INTEROPERABILITY.md).
