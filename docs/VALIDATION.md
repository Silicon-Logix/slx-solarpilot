# Verifiche ripetibili

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.


La compilazione esegue nove gruppi CTest sul programma e sull'SDK pubblici.

```powershell
ctest --test-dir build/ucrt --output-on-failure
```

Le verifiche comprendono:

- bilanci di potenza ed energia, rendimento, SOC e limiti fisici
- strategie EMS, priorità, finestre e protezione dei cicli
- host asincrono, registrazioni e riproduzione delle sessioni
- oracoli numerici indipendenti e scenari deterministici
- separazione geometrica delle superfici della scala
- integrità e conservazione delle release
- interoperabilità TCP SunSpec e gestione delle richieste non valide
- comunicazione EEBUS SHIP/SPINE tra processi e perdita del peer
- funzionamento con un controllore esterno al processo del simulatore

Le prove usano istanze e dati separati dalle configurazioni dell'utente.
I risultati riguardano gli scenari verificati; non costituiscono una
certificazione EEBUS/SunSpec o una qualifica di dispositivi commerciali.
