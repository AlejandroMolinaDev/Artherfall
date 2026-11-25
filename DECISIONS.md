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

### 2026-11-25 · Motor de modulación (F5)
- **Decisión:** `ModulationEngine` evalúa todas las fuentes una vez por trozo de control y calcula los valores
  efectivos en el dominio normalizado (0..1) de cada parámetro: `norm(host) + Σ macro·amount + Σ matriz`, con
  saturación a [0,1]. La rutina (fuentes/destinos/curvas/vía de la matriz, mapas de macros, pasos de los
  secuenciadores, formas custom de LFO) vive en nodos del estado (`MODMATRIX`, `MACROS`, `SEQ`, `LFOSHAPES`, por
  IDs string) y llega al audio thread por un `TripleBuffer` (sin locks; el lado escritor usa un SpinLock propio).
  Los ajustes de los moduladores se leen de los valores efectivos del trozo anterior (modulación de moduladores con
  1 trozo de retardo). `glob_moddepth` (destino de DRIFT) escala LFO/Env/Seq/Chaos/Analyzer; macros y MIDI no.
- **Motivo:** cantidades homogéneas para cualquier destino ("la misma fracción del recorrido del knob"), nada de
  allocations en el audio thread y estado completo guardado con el proyecto.

### 2026-11-25 · Macros como tabla propia (4 × 8 destinos), aparte de la matriz
- **Decisión:** las macros tienen su propio mapa de destinos (los valores por defecto siguen la spec: HAZE, DRIFT,
  GRIT, BLOOM) y además están disponibles como fuentes de la matriz.
- **Motivo:** los 32 slots de la matriz quedan libres para el usuario; los presets pueden redefinir los mapas.

### 2026-11-25 · MIDI a nivel de bloque
- **Decisión:** nota, velocity, mod wheel (CC1) y pitch bend se leen una vez por bloque del host; el note-on
  dispara el retrigger "MIDI Note" de los LFOs.
- **Motivo:** para fuentes de modulación y retrigger la precisión de bloque es suficiente y mucho más simple.

### 2026-11-25 · Input / Output Stage (F6)
- **Decisión:** `InputStage` acondiciona solo el camino wet (la señal seca del dry/wet final queda intacta).
  `OutputStage`: wet → tilt (shelves ±tilt/2 a 800 Hz) → HPF/LPF → width → glue (pico enlazado, knee 6 dB,
  ataque/release sobre la reducción) → ducker (envolvente de la entrada seca ALINEADA con el wet, hasta −30 dB)
  → dry/wet equal-power → soft clipper (lineal hasta 0.8 ≈ −1.9 dBFS) → output gain → limitador de seguridad sin
  lookahead (ataque instantáneo, release 80 ms, recorte duro final al ceiling) → bypass. Filtros en "Off" se
  desvanecen (sin desfase). Medidores en `Meters` (atomics) para la GUI.
- **Motivo:** el limitador sin lookahead no añade latencia y garantiza el techo (verificado por test); el ducker
  sobre la señal seca alineada mantiene la coherencia cuando el Spectral añade latencia.

### 2026-11-25 · GUI: escalado por AffineTransform y estado de UI en el ValueTree
- **Decisión:** todas las vistas se maquetan al tamaño base 1000×640 dentro de un componente `content` al que se
  aplica `AffineTransform::scale` (75–200 %, relación de aspecto fija). La escala y la pestaña activa se guardan
  en el nodo `UI` del estado. Todo se dibuja con paths (sin bitmaps) → nítido en HiDPI/Retina.
- **Motivo:** un único layout que escala de forma exacta y simple; el plugin reabre con el tamaño elegido.

### 2026-11-25 · Capturas de la GUI sin pantalla
- **Decisión:** test oculto `[.gui]` que renderiza cada vista con `createComponentSnapshot` a PNG
  (`AETHERFALL_SNAPSHOT_DIR=... AetherfallTests "[.gui]"`); un test normal `[gui]` abre/redimensiona/cierra el
  editor varias veces.
- **Motivo:** el contenedor de desarrollo no tiene display; así se puede revisar el diseño en cada sesión.

### 2026-11-25 · Undo/redo por instantáneas, no con `juce::UndoManager` en el APVTS
- **Decisión:** `StateHistory` guarda una copia del estado completo ANTES de cada gesto de UI (vía
  `AudioProcessorParameter::Listener::parameterGestureChanged`) y antes de cada edición de estado (cadena,
  matriz, macros, gate, secuencias, formas de LFO). 100 pasos. El APVTS sigue sin UndoManager.
- **Motivo:** con un UndoManager en el APVTS la automatización del host también se grabaría en el historial
  (el APVTS vuelca los parámetros al árbol con el UndoManager). Así solo se deshace lo que hizo el usuario.

### 2026-11-25 · Modulación desde la UI (F8)
- **Decisión:** las fuentes son "chips" arrastrables (vista MOD); los knobs aceptan el drop (`modsrc:<id>`) y
  asignan el primer slot libre de la matriz (cantidad 50 %). Las pestañas se abren al pasar por encima durante el
  arrastre, para soltar sobre knobs de cualquier vista. Clic derecho en un knob: "Modulate with ▸", "Remove
  modulation", "Reset". Anillo naranja en tiempo real = desplazamiento de modulación actual (offset publicado por
  el `ModulationEngine` en atomics).
- **Motivo:** flujo estilo Serum/Vital con una GUI de pestañas; el menú contextual cubre el caso sin arrastre.

### 2026-11-25 · Analizador de espectro y visualizador por software
- **Decisión:** el procesador publica el wet (mono) en un `AbstractFifo` de 8192 muestras; el visualizador de MAIN
  calcula una FFT de 2048 a 30 fps (solo con la vista visible) y dibuja 96 bandas log. Sin OpenGL por ahora.
- **Motivo:** coste mínimo y cero dependencias; el renderizado OpenGL opcional queda para F10 si hiciera falta.

### 2026-11-25 · Presets (F9)
- **Decisión:** presets de fábrica como tabla de datos en C++ (solo diferencias respecto a los defaults + cadena +
  modulación + patrón de gate); al cargar uno se reconstruye el estado completo desde los valores por defecto
  (`PresetManager::buildFactoryState`) y se aplica como un único paso de undo. Presets de usuario = XML
  `<AETHERFALL_PRESET name category version>` con el estado completo (sin el nodo `UI`), extensión `.afpreset`,
  en `Documents/AETHERFALL/Presets`; favoritos en `favourites.xml`. El preset actual (nombre + categoría) se
  guarda en el nodo `PRESET` del estado. El color de acento de la UI sigue a la categoría.
- **Motivo:** presets versionables y revisables en el repo, sin ficheros binarios; los de fábrica no dependen del
  disco; un test garantiza que los 63 cargan y procesan audio limpio.

### 2026-11-25 · Cabecera de dos filas
- **Decisión:** fila 1 = logo, barra de presets (◀ nombre | categoría ▶), undo/redo, A/B, RAND, bypass; fila 2 =
  pestañas. Altura de cabecera 84 px (las vistas tienen 556 px a escala 1).
- **Motivo:** espacio para el navegador de presets sin comprimir las pestañas.

### 2026-11-25 · Optimización (F10)
- **Decisión:** (1) trabajo a tasa de control donde no se oye: Resonator y Drone calculan frecuencias, ganancias de
  lazo y paneos una vez por trozo (glide por trozo); (2) `dsp::sinCycles/cosCycles` (reducción a cuarto de periodo
  + polinomio de 7º orden, error ~2e-4) en LFOs internos y osciladores (ModFX, Stereo, Tape, Comb, LoFi, Reverse,
  Texture, Drone); (3) Spectral sin atan2/sin/cos por bin: fasores unitarios, tabla de rotación por bin para el
  freeze y ganancias de tilt cacheadas; (4) coeficientes del phaser cada 4 muestras; filtro del viento por trozo;
  (5) el pitch tracking del Input Analyzer solo corre si alguna ruta usa "In Pitch" o el Comb está en Track.
- **Calidad (`glob_quality`):** Eco = lecturas lineales en el FDN, Saturator sin oversampling, Spectral a FFT 2048,
  Granular ≤ 32 granos, chorus ≤ 3 voces. Normal = lo configurado. High = Saturator a 4x. La latencia reportada no
  cambia con la calidad.
- **Resultado (`[.bench]`, 48 kHz, este contenedor):** cadena típica 5.1 % → 4.2 %; cadena extrema de 8 módulos
  pesados 10.4 % → 7.7 %; core solo 1.9 % (3.3 % con shimmer). Meta de la spec (< 8 %) cumplida.

### 2026-11-25 · Entrega (F11)
- **Decisión:** workflow `release.yml` disparado por tags `v*`: compila Windows y macOS (Universal) con LTO y CLAP,
  empaqueta zips (VST3 + CLAP + Standalone; en macOS también AU, firmado ad-hoc) y publica un GitHub Release con
  notas generadas. CLAP vía `clap-juce-extensions` fijado a un commit (opción `AETHERFALL_BUILD_CLAP`, activada en
  CI y releases). La creación de tags/releases la decide el usuario.
- **Motivo:** binarios descargables y reproducibles sin pasos manuales; el formato CLAP se valida en cada push.

### 2026-11-25 · Tests de host contra el binario VST3
- **Decisión:** ejecutable `AetherfallHostTests` (tests/host) que carga el `.vst3` compilado con el hosting VST3 de
  JUCE y recorre escenarios de DAW (los de la checklist de FL Studio que no requieren FL). Se ejecuta en `ctest`
  y en el CI; en Linux con Xvfb + openbox para poder abrir ventanas.
- **Hallazgo:** si un host envía bloques mayores que el `maxSamplesPerBlock` anunciado (prohibido por la spec
  VST3), el envoltorio VST3 de JUCE desborda su buffer interno (corrupción de heap dentro de JUCE, no en el DSP de
  AETHERFALL, que sí acepta cualquier tamaño de bloque — probado en los tests unitarios). FL Studio respeta el
  máximo en VST3; se deja documentado por si algún host no lo hiciera.
