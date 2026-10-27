# Strategie EMS

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.


Le strategie coordinano produzione, accumulo, ricarica EV e richieste dei carichi
all'interno dei limiti dell'impianto simulato.

## Accumulo e autoconsumo

La gestione bilanciata conserva la riserva SOC e limita carica e scarica in base
alla potenza e all'energia disponibili. Il limite di immissione può ridurre la
produzione fotovoltaica; l'energia non prodotta è contabilizzata separatamente.
La carica programmata rispetta la finestra e il SOC obiettivo impostati.

## Ricarica EV

Sono disponibili surplus, ricarica rapida e obiettivo di partenza. Il modello
monofase usa 230 V, minimo di avvio 6 A e gradini di corrente di 1 A. La gestione
segnala quando tempo e potenza disponibili non consentono di raggiungere
l'obiettivo; non inventa energia o capacità disponibili.

## Carichi

Priorità e finestre governano i nuovi avvii di lavatrice e lavastoviglie. Il
margine disponibile considera anche le successive fasi di riscaldamento dei
cicli già autorizzati. Un ciclo protetto prosegue durante la riduzione della
potenza flessibile; un'indisponibilità esplicita può arrestarlo.

Forno, piano cottura e bollitore possono determinare un sovraccarico. La soglia
di rete è un limite di gestione e segnalazione, non un interruttore ideale
che elimina i consumi.

## Policy e confronto

La policy pubblica in `examples/policy.cpp` consente di valutare una diversa
priorità dell'EV. Il confronto esegue istanze indipendenti dello stesso scenario;
mostra energie e SOC finali per distinguere consumo, autoconsumo e servizio reso.

Le tariffe eventualmente inserite dall'operatore sono valori illustrativi.
Previsioni operative, tariffe dinamiche e integrazioni specifiche sono presentate
come sviluppi su richiesta. Dettagli e convenzioni in [Modello](MODEL.md).
