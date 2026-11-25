# DECISIONS — registro de decisiones técnicas

Formato: fecha · decisión · motivo.

---

### 2026-11-25 · JUCE 8.0.17 (`8.0.17-security`) vía FetchContent
- **Decisión:** fijar JUCE en la etiqueta `8.0.17-security` (última estable a esta fecha) y obtenerla con
  `FetchContent` (shallow). Alternativas soportadas: `libs/JUCE` o `-DAETHERFALL_JUCE_DIR`.
- **Motivo:** la 8.0.17 incluye endurecimiento de parsers XML/JSON/ValueTree, útil porque leeremos presets
  y estados de proyecto. FetchContent evita submódulos y mantiene el repo ligero.

### 2026-11-25 · Identidad del plugin
- **Decisión:** `PRODUCT_NAME "AETHERFALL"`, `COMPANY_NAME "Aetherfall Audio"`, manufacturer code `Aefl`,
  plugin code `Afx1`, bundle id `com.aetherfallaudio.aetherfall`.
- **Motivo:** códigos únicos de 4 caracteres; **no cambiarlos nunca** (FL/Logic identifican el plugin por ellos
  y los proyectos guardados dejarían de encontrarlo).

### 2026-11-25 · Entrada MIDI activada y AU como `aumf` (Music Effect)
- **Decisión:** `NEEDS_MIDI_INPUT TRUE`, `AU_MAIN_TYPE kAudioUnitType_MusicEffect`.
- **Motivo:** la spec prevé MIDI como fuente de modulación opcional (FL puede enrutar MIDI a FX). En AU un efecto
  que recibe MIDI debe ser `aumf`. Cambiar el tipo AU después rompería proyectos de Logic, así que se fija ya.

### 2026-11-25 · Código compartido plugin/tests mediante librería INTERFACE
- **Decisión:** `AetherfallShared` (INTERFACE) lleva módulos JUCE, defines y flags; el plugin y el runner de
  tests (`juce_add_console_app`) compilan las mismas fuentes (glob con `CONFIGURE_DEPENDS`).
- **Motivo:** enlazar el static lib `SharedCode` de JUCE en otra app causa símbolos duplicados de los módulos;
  compilar dos veces es más lento pero robusto. El glob recoge los módulos nuevos de fases futuras sin tocar CMake.

### 2026-11-25 · Runtime MSVC estático
- **Decisión:** `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded[Debug]`.
- **Motivo:** el `.vst3` no depende del redistribuible de VC++ en la máquina del usuario.

### 2026-11-25 · Catch2 v3 con `main` propio
- **Decisión:** tests con Catch2 v3.16.1; `tests/TestMain.cpp` crea `juce::ScopedJuceInitialiser_GUI`.
- **Motivo:** APVTS/ValueTree y timers necesitan MessageManager; así los tests se parecen al entorno de un host.

### 2026-11-25 · pluginval solo en CI
- **Decisión:** pluginval (v1.0.4, nivel 10) y auval se ejecutan en GitHub Actions, no en el contenedor de
  desarrollo de Claude.
- **Motivo:** en el entorno de las sesiones no está permitido ejecutar el binario descargado de pluginval; el CI
  lo valida en Windows, macOS y Linux en cada push.
