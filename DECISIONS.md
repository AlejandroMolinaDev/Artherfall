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

### 2026-11-25 · Tabla de parámetros única (X-macro) en `Source/Parameters/ParamList.h`
- **Decisión:** todos los parámetros (~450) se declaran una sola vez en una X-macro que genera el enum `PID`,
  la tabla constexpr `paramInfos` (rango, default, unidad, flags) y el layout del APVTS.
- **Motivo:** un único punto de verdad; el DSP indexa valores por `PID` (array plano, sin búsquedas por string
  en el audio thread) y los presets podrán definirse como tablas de datos.
- **Regla:** los IDs string son lo que se guarda en proyectos/presets → no renombrar desde F9. Las listas de
  opciones solo crecen al final. Nombres ≤ 16 caracteres (test automático).

### 2026-11-25 · On/Mix/Solo/Parallel pertenecen al MÓDULO, no al slot
- **Decisión:** cada uno de los 18 módulos tiene sus parámetros `<mod>_on/_mix/_solo/_par`. El orden de la
  cadena (hasta 8 tipos de módulo) se guarda como estado (ValueTree), no como parámetro automatizable.
  Un mismo tipo de módulo no puede aparecer dos veces en la cadena.
- **Motivo:** la automatización de FL sigue al módulo cuando se arrastra a otro slot; nombres legibles
  ("Gran Mix" en vez de "Slot 3 P4"); automatizar el tipo de módulo de un slot no tiene sentido musical.

### 2026-11-25 · Control rate de 64 muestras
- **Decisión:** el procesador divide cada bloque del host en trozos de ≤ 64 muestras; parámetros y modulación
  se evalúan una vez por trozo (`ModContext`), los módulos suavizan internamente (rampas por muestra).
- **Motivo:** el comportamiento no depende del tamaño de buffer del host (FL usa buffers variables y a veces
  mayores que el anunciado); permite modulación rápida y barata.

### 2026-11-25 · Matriz de modulación: cantidades como parámetros, rutas como estado
- **Decisión:** `mod1_amt … mod32_amt` son parámetros automatizables; fuente, destino, curva y "vía" se guardan
  en el ValueTree. Igual para patrones del Trance Gate, pasos de los secuenciadores y formas custom de LFO.
- **Motivo:** un parámetro "destino" con ~400 opciones sería inutilizable en la lista de FL; el estado completo
  se guarda igualmente con el proyecto y en presets.

### 2026-11-25 · Oversampling con IIR polifásico de JUCE
- **Decisión:** `dsp::Oversampler` (1x/2x/4x) usa `filterHalfBandPolyphaseIIR`; ambos motores se crean en prepare.
- **Motivo:** latencia prácticamente nula y cambio de factor sin allocations; el pequeño desfase frente a la
  señal seca del propio módulo es despreciable en este contexto (efectos de saturación sobre atmósferas).

### 2026-11-25 · ATMOS CORE: FDN 16 líneas con Hadamard y shimmer dentro del lazo
- **Decisión:** tanque FDN de 16 líneas (double), matriz Hadamard normalizada (FWHT), ganancia por línea
  `g = 10^(-3·L/(RT60·fs))`, shelves HF/LF por línea, all-pass "late" dentro de cada línea, modulación con
  osciladores por fasor (sin trig por muestra). El shimmer lee la MISMA línea con dos cabezas granulares
  (`GranularHeads`) y se mezcla dentro del lazo (líneas 0–7): la energía desplazada sigue escalada por g < 1,
  así que nunca se desboca. Freeze: g → 0.999995, shelves planos, entrada a 0.
- **Motivo:** estable por construcción, densidad alta, coste ≈ 2 % (sin shimmer) / 3.4 % (con shimmer) de un
  núcleo a 48 kHz (medido con el test `[.bench]`).

### 2026-11-25 · Compensación de nivel del ATMOS CORE
- **Decisión:** `outputScale = 3.2 · (RT60/3)^-0.38 · (sizeScale/0.578)^0.5 · makeup(color)`.
- **Motivo:** el nivel bruto del tanque variaba ~25 dB con Decay/Size; con la compensación el wet queda entre
  −2 y −6 dB re. entrada para Decay ≥ 2 s (test `[.measure]`), dejando que las colas largas suenen algo mayores.

### 2026-11-25 · Mezclas dry/wet de potencia constante
- **Decisión:** `core_mix` y `out_mix` usan cos/sin (equal-power).
- **Motivo:** la señal wet de una reverb está decorrelada de la seca; equal-power mantiene la sonoridad estable
  al mover el knob.

### 2026-11-25 · FXChain: instancias fijas, solo/parallel y reordenado por fundido
- **Decisión:** `FXChain` crea en `prepare()` una instancia de cada uno de los 18 módulos; el orden activo es un
  `uint64` (1 byte por slot) leído con un único `atomic::load` por trozo. Un módulo que pasa de apagado a activo
  (o entra en la cadena) se `reset()`ea antes de procesar. Solo: si algún módulo activo tiene solo, el resto pasa
  la señal. Parallel: un slot paralelo recibe la entrada del slot serie anterior; el grupo se promedia con pesos
  (cabeza = 1, ramas paralelas = su ganancia on/off suavizada). Reordenar: la salida de la cadena se funde hacia
  su entrada (10 ms), se aplica el nuevo orden y se funde de vuelta.
- **Motivo:** cero allocations en el audio thread, estado de los módulos conservado al moverlos, transiciones sin
  clicks verificadas por test (`[chain]`).

### 2026-11-25 · Módulos aún no implementados = stub pass-through
- **Decisión:** `createModule()` devuelve un `PassthroughModule` para los módulos de F4; `isModuleImplemented()`
  lo indica (el editor provisional los marca con "(F4)").
- **Motivo:** el proyecto compila y la cadena funciona en todo momento mientras se completan los módulos.

### 2026-11-25 · Granular Cloud con buffer mono
- **Decisión:** el buffer circular (~8 s, potencia de 2) guarda la suma mono de la entrada (+ feedback); cada grano
  se panoramiza dentro de "Width". Normalización de nivel por `1/sqrt(densidad·tamaño)`.
- **Motivo:** mitad de memoria y lecturas, y las nubes ganan anchura por el paneo aleatorio por grano.

### 2026-11-25 · Latencia: Spectral siempre 4096 muestras, compensada en cadena y en la señal seca
- **Decisión:** el módulo Spectral reporta siempre 4096 muestras (también con FFT 2048, rellenando). En la
  cadena, los módulos con latencia se procesan siempre mientras están en ella (aunque estén apagados), la señal
  seca del slot se retrasa igual (`dryComp`) y las ramas de un grupo paralelo se alinean con la más lenta
  (`groupComp`). El procesador retrasa la señal seca del dry/wet final y llama a `setLatencySamples()` cuando la
  latencia de la cadena cambia (cambio de delay de la señal seca con fundido de 20 ms).
- **Motivo:** la latencia solo existe si el Spectral está en la cadena (el resto de usos queda sin latencia) y
  no cambia al cambiar el tamaño de FFT (cambiar latencia en caliente obliga al host a recompensar). Verificado
  por test: alineación exacta dry/wet con latencia 4096.

### 2026-11-25 · Patrón del Trance Gate como estado
- **Decisión:** nodo `GATE` (propiedad `pattern`, 16 bits, bit 0 = paso 1) + `std::atomic<uint32_t>`; llega a
  los módulos en `ModContext::gatePattern`.
- **Motivo:** mismo patrón que el orden de la cadena: se guarda con el proyecto/presets, editable desde la GUI
  (F7/F8) y con undo, sin 16 parámetros de automatización extra.

### 2026-11-25 · Pitch tracking YIN propio (`Source/DSP/Analysis/PitchTracker`)
- **Decisión:** YIN sobre la señal decimada a ~12 kHz, ventana 1024, hop 256, rango ~45 Hz–1.5 kHz, umbral 0.15.
  El Drone usa su propia instancia sobre la entrada de su slot; el Input Analyzer de F5 reutilizará la clase.
- **Motivo:** robusto para voces/bajos/pads, coste acotado y sin allocations en el audio thread.
