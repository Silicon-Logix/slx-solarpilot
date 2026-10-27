# SLX SolarPilot 0.4.5

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.

Simulatore energetico residenziale per Windows x64 con abitazione 3D,
fotovoltaico, accumulo, elettrodomestici e ricarica EV.

- Produzione solare con variazioni di ora, stagione, temperatura e nuvole.
- Bilanci di potenza ed energia, rendimento e stato di carica degli accumulatori.
- Strategie EMS, priorità dei carichi, finestre operative e gestione della ricarica.
- Collegamenti EEBUS SHIP/SPINE e SunSpec Modbus TCP con monitor dei messaggi.
- Scenari, confronto delle policy, registrazione, riproduzione ed esportazione CSV.
- About, guida, logo Silicon LogiX e vista 3D con correzione delle superfici sovrapposte.

Il pacchetto include il programma portabile, l'interfaccia e l'SDK pubblici,
le librerie compilate del core, gli asset runtime necessari e le licenze.
Estrarre l'intero ZIP e avviare **SLX-SolarPilot.exe**. Sono richiesti
Windows 10/11 x64 e OpenGL 3.3. Il file `.zip.sha256` permette di verificare
l'integrità del download.

La compilazione dell'interfaccia usa il core binario fornito e dipendenze
con versioni e hash fissati. Le nove suite verificano modello energetico,
calcoli elettrici indipendenti, EMS, registrazione, geometria delle scale,
conservazione delle release e comunicazione tra processi.

I valori sono simulati. La compatibilità con dispositivi esterni dipende dai
modelli e dai casi d'uso supportati e non costituisce una certificazione EEBUS
o SunSpec. Le integrazioni **Su richiesta** richiedono uno sviluppo dedicato.

Distribuzione di valutazione soggetta alla licenza SLX. I componenti di terzi
mantengono le rispettive licenze e attribuzioni. Firmware e sorgenti del core
riservato sono esclusi.

[Informazioni e integrazioni Silicon LogiX](https://www.siliconlogix.it/contatti)
