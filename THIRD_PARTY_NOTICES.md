# Componenti di terze parti

Copyright (c) 2026 Marco Pezzullo — Silicon LogiX. Licenza: Silicon LogiX Evaluation License 1.0. Vedere LICENSE.

Le condizioni Silicon LogiX si applicano alle sole parti originali. Le licenze
seguenti rimangono indipendenti. Le versioni e gli hash dei download sono in
`scripts/dependencies.lock.json` e nel manifest di release.

| Componente | Versione fissata | Licenza / file |
| --- | --- | --- |
| GLFW | 3.4 | zlib, `licenses/glfw-LICENSE.txt` |
| Dear ImGui | 1.92.4-docking | MIT, `licenses/imgui-LICENSE.txt` |
| ImPlot | 0.17 | MIT, `licenses/implot-LICENSE.txt` |
| cgltf | 1.15 | MIT, `licenses/cgltf-LICENSE.txt` |
| stb | f0569113c93ad095470c54bf34a17b36646bbbb5 | MIT/public domain, `licenses/stb-LICENSE.txt` |
| nlohmann/json | 3.12.0 | MIT, `licenses/json-LICENSE.MIT` |
| NNG | tag v1.12.0 (header upstream 1.11.0) | MIT, `licenses/nng-LICENSE.txt` |
| LLVM/libc++ runtime | llvm-mingw 20261020 | Apache-2.0 with LLVM exceptions, `licenses/llvm-runtime-LICENSE.txt` |
| MinGW-w64 support | runtime fornito dalla toolchain | `licenses/mingw-w64-COPYING` |

La toolchain completa non viene distribuita nella release applicativa: lo script
di bootstrap la scarica separatamente con hash fissato. Segoe UI viene caricato
dall'installazione Windows dell'utente e non viene ridistribuito.

I riferimenti PVGIS provengono dalla Commissione europea, Joint Research Centre.
URL, parametri e hash originali sono in `SLX_SolarPilot/config/pvgis_sources.json`.
Sono dati storici modellati, senza dichiarazioni di endorsement da parte del JRC.

Gli avvisi dell'asset pack, compreso il font usato nelle texture originali, sono
conservati in `SLX_SolarPilot/docs/NOTICE.txt` e `DejaVu_font_notice.txt`.


## Bridge EEBUS 0.2

Lo stack [Enbility eebus-go](https://github.com/enbility/eebus-go),
[ship-go](https://github.com/enbility/ship-go) e
[spine-go](https://github.com/enbility/spine-go) resta sotto licenza MIT.
L'integrazione originale SLX e il modello non cambiano le licenze dello stack.
Revisioni e checksum dei moduli: `scripts/eebus.dependencies.lock.json`.
Le licenze dei moduli diretti e transitivi, con testo originale integrale, sono
in `licenses/eebus/`; include anche moduli usati solo da test/tool upstream.
Il runtime Go è BSD, in `licenses/eebus/Go-LICENSE.txt`. Go è collegato nel
bridge; la toolchain non viene distribuita. La build usa `CGO_ENABLED=0` e
non richiede un runtime Go installato sul PC dell'utente.

La revisione eebus-go indicata nel lock include una modifica SLX alla gestione
concorrente delle entità durante disconnessione e riconnessione. Ricerca e
modifica degli scenari avvengono nella stessa sezione protetta; le letture
restituiscono copie. Licenza MIT e attribuzioni originali sono conservate.
Provenienza e SHA256 sono nel lock; vedere `licenses/eebus/SLX-MODIFICATIONS.txt`.

## Modelli SunSpec 0.4

Definizioni JSON originali [SunSpec Alliance](https://github.com/sunspec/models),
commit 90b4a331dcca1d6eac69c1bead952fddcc5852e0, incorporate invariate nel servizio.
`licenses/sunspec-models-LICENSE.txt`. Provenienza e SHA256 in
`scripts/sunspec.models.lock.json`. Le modifiche e l'implementazione server/client
sono codice SLX distinto dalle definizioni standard. Nessuna certificazione o
approvazione SunSpec è implicata. Il client pysunspec2 è scaricato separatamente
solo per le verifiche di sviluppo e non è distribuito nel runtime.
