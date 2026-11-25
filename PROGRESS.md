# PROGRESS — estado del proyecto

> Este archivo es la memoria entre sesiones. Actualizar SIEMPRE antes de cerrar una sesión.

## Estado actual
- **Fase:** F7 – GUI base **terminada** → siguiente: **F8 – GUI avanzada**.
- Última actualización: 2026-11-25 (sesión 1).

## Terminado
- **F0 – Base:** CMake + JUCE 8.0.17 (FetchContent) + Catch2 v3.16.1; VST3/AU/Standalone; mono→estéreo y
  estéreo→estéreo; editor provisional (header + `GenericAudioProcessorEditor`); CI GitHub Actions
  (Linux/Windows/macOS universal: build, tests, pluginval nivel 10, auval); README de compilación/instalación.
  CI verificado: Linux y Windows en verde con pluginval L10 (el job de macOS se canceló sin arrancar, ver bugs).
- **F1 – Parámetros y utilidades:**
  - `Source/Parameters/ParamList.h` (X-macro, ~450 parámetros), `ParamIDs.h` (enum `PID`, `paramInfos`,
    `ModuleType`, `moduleInfos`, helpers `lfoParam()` etc.), `ChoiceLists.h`, `NoteDivision.h`,
    `ParamLayout.{h,cpp}` (APVTS, texto↔valor con unidades, `ParameterCache`).
  - `Source/DSP/Utils/`: `Smoothers.h` (Linear, OnePole, ControlRamp), `DCBlocker.h`, `DelayLine.h`
    (tap lineal/cúbico; convención documentada), `Filters.h` (OnePole, SVF TPT, Biquad RBJ, Allpass1),
    `Oversampler.h` (1x/2x/4x IIR), `DspMath.h`, `Interp.h`, `Random.h`.
  - `Source/DSP/Chain/IModule.h` (+ `ModuleBase`), `ModContext.h` (valores efectivos por `PID`, transporte,
    `AnalysisFrame`, helpers de tempo).
  - Procesador: trozos de control de 64 muestras, transporte, `ModContext`, parámetro de bypass del host.
- **F2 – ATMOS CORE:** `Source/DSP/Core/`
  - `FDN.h` (FDN16), `Diffuser.h` (all-pass Schroeder + cadenas), `PitchShifter.h` (`GranularHeads` +
    `PitchShifter` autónomo, reutilizable por el Shimmer Delay de F4), `AtmosCore.{h,cpp}`.
  - Pre-delay (ms o sync, máx. 2 s), bloom (swell por envolventes + difusores largos), difusión early/late,
    modulación, damping HF/LF, shimmer A/B con blend, freeze, 5 color modes (Clean/Dark/Vintage 80s/Lo-Fi/Void),
    width, core mix, DC blocker, compensación de nivel. Tail reportado al host (∞ en freeze).
  - Integrado en el procesador con una etapa de salida PROVISIONAL (dry/wet + output gain + bypass suave)
    en `AetherfallProcessor::processChunk` — se sustituye por el Output Stage real en F6.
- **Tests (30 casos, todos verdes):** tabla de parámetros, utilidades, RT60 medido vs Decay, estrés de todos los
  colores con todas las señales (sin NaN/Inf, pico acotado), freeze estable 10 s, mix 0 transparente, reset,
  sample rates 44.1–192 kHz, bloques de tamaño variable (incluso > al anunciado), mono→estéreo, bypass,
  colas tras parar la entrada, save/restore de estado. Ocultos: `[.measure]` (niveles), `[.bench]` (CPU).

- **F3 – Chain + primeros módulos:**
  - `Source/DSP/Chain/ChainOrder.h` (orden: pack/unpack a `uint64`, texto `"granular,,filter"`, sanitize sin
    duplicados), `FXChain.{h,cpp}` (8 slots, on/off y mix suavizados, solo, parallel con promedio ponderado,
    reordenado con fundido de 10 ms, reset de módulos al (re)activarse, latencia y tail agregados).
  - Procesador: nodo `CHAIN` (propiedad `order`) en el estado del APVTS + espejo `std::atomic<uint64_t>`;
    `setChainOrder()/getChainOrder()/getActiveChainOrder()`; `ValueTree::Listener` (cambios, redirect, carga de
    estado). Flujo actual: entrada → FX chain → ATMOS CORE → dry/wet provisional.
  - `Source/DSP/Modules/`: `ModuleFactory` (+ stub pass-through para los de F4), `GranularCloud` (64 granos,
    buffer 8 s, pitch aleatorio libre/octavas/oct+5ª, reversa, 3 ventanas, freeze, feedback, width),
    `TapeDelay` (glide de tiempo, wow con deriva + flutter, saturación de cabeza de grabación, "age" por
    repetición, feedback hasta 105 %, sync 1/64–16 compases), `FilterSweep` (LP/HP/BP/Notch/Formant, 12/24 dB,
    SVF o Ladder ZDF con tanh autooscilante, drive, offset estéreo), `Saturator` (Tape/Tube/Diode/Fold/Fuzz,
    oversampling 1/2/4x, bias, tilt, auto-gain RMS), `StereoEngine` (M/S width, Haas, auto-pan, orbit, bass
    mono LR4).
  - `Source/DSP/Utils/TempoPhase.h`: fase de LFO libre o sincronizada (bloqueada a la posición de la canción
    cuando el transporte corre).
  - Editor provisional: fila de 8 selectores de slot para probar la cadena (los módulos de F4 salen como "(F4)").
- **Tests: 43 casos, todos verdes.** Nuevos: estrés de los 18 módulos con parámetros aleatorios × 5 señales,
  saturador (auto-gain ±2 dB, silencio), filtros (LP/HP SVF y Ladder), stereo (width 0 = mono, LR4 plano),
  tape (eco a tiempo exacto), granular (límite de voces, freeze), cadena (orden, vacío transparente,
  reordenar sin clicks ×2, solo, parallel), orden de cadena guardado en el estado.
  CPU (`[.bench]`): cadena típica de 5 módulos + core con shimmer = 3.9 % de un núcleo a 48 kHz.

- **F4 – Módulos restantes (13):** en `Source/DSP/Modules/`: `Bitcrush`, `RingFreq` (ring + freq shifter SSB con
  Hilbert IIR), `ModFX` (phaser 2–12 etapas / flanger / chorus 6 voces), `ShimmerDelay`, `MultiTap` (8 taps con
  forma y difusión progresiva, compensando el retardo de los difusores), `Comb` (Karplus con smear y gancho de
  pitch tracking), `TranceGate` (patrón en el nodo `GATE` del estado), `ReverseSwell` (dos cabezas en reversa con
  fundido, modo swell), `TextureLayer` (vinilo, hiss, viento, lluvia, hum procedurales + envelope follower),
  `LoFiTape`, `ResonatorBank` (4–8 resonadores de cuerda en acordes de la tonalidad global), `DroneGenerator`
  (YIN + cuantización a escala + voicings + osciladores sine→saw PolyBLEP + ruido + LP resonante),
  `SpectralFreeze` (STFT 2048/4096, smear, freeze con fase que avanza, blur, shimmer espectral, tilt; latencia
  fija 4096).
  - Nuevos utils: `Source/DSP/Utils/MusicTheory.h` (escalas, cuantización), `Source/DSP/Analysis/PitchTracker`.
  - Latencia: `FXChain` compensa dry de slot y ramas paralelas; el procesador retrasa la señal seca y reporta
    `setLatencySamples()` (0 sin Spectral, 4096 con Spectral).
  - Procesador: API `setGatePattern()/getGatePattern()`.
- **Tests: 58 casos, todos verdes** (+ comportamiento de cada módulo nuevo, identidad del STFT con retardo exacto,
  freeze espectral, drone sigue la nota A3→A2 y sostiene, latencia reportada y alineación dry/wet).
  CPU (`[.bench]`): cadena típica 3.7 %; cadena extrema de 8 módulos pesados (Spectral, Granular, Drone,
  Resonator, Shimmer Delay, Multi-Tap, ModFX, Saturator 4x) + core = 9.3 % de un núcleo → optimizar en F10.

- **F5 – Modulación:**
  - `Source/DSP/Modulation/`: `ModSources.h` (29 fuentes con IDs string, curvas Linear/Exp/Log/S-Curve),
    `ModRouting.h` (slots de matriz, mapas de macros 4×8 con los valores por defecto de la spec, pasos de
    secuenciador, formas custom de LFO), `ModulatorSources.h` (`Lfo` 8 formas/sync bloqueado a la canción/fase/
    retrigger Free-Transport-Transient-MIDI/polaridad/smooth, `EnvFollower` input o wet, `StepSeq` con glide,
    `Chaos` Lorenz / drunk walk), `ModulationEngine.{h,cpp}`.
  - `Source/DSP/Analysis/InputAnalyzer.{h,cpp}`: RMS, pico, transitorios (onset), pitch (YIN), brillo
    (estimación de centroide) → `ModContext::analysis` y fuentes del analizador.
  - `Source/DSP/Utils/TripleBuffer.h`; `Source/Parameters/ModRoutingState.{h,cpp}` (ValueTree ↔ `ModRouting`).
  - Procesador: `getModRouting()`, `setModSlot()`, `setMacroTarget()`, `setSeqSteps()`, `setLfoShape()` (todas
    aceptan `UndoManager*` para F8), `getModSourceValue()` (para la GUI), lectura de MIDI, señal wet del trozo
    anterior para los env followers "Wet".
- **Tests: 65 casos, todos verdes** (+ LFO sincronizado cae en la fase exacta, rangos de las 8 formas, chaos y
  step seq, analizador (onset, RMS, brillo de 1 kHz), matriz y macros dentro de rango, Mod Depth 0, routing
  guardado/recuperado, LFO modulando la ganancia de salida en el procesador). CPU cadena típica: 4.3 %.

- **F6 – Input / Output Stage:**
  - `Source/DSP/Core/InputStage.{h,cpp}`: gain, HPF/LPF (Off en los extremos, con fundido), bass mono LR4,
    pre-drive. Procesa solo el camino wet.
  - `Source/DSP/Core/OutputStage.{h,cpp}`: tilt, HPF/LPF, width, glue compressor, ducker (por la entrada seca
    alineada), dry/wet, soft clipper, output gain, limitador de seguridad, bypass; sustituye a la etapa provisional.
  - `Source/DSP/Utils/Meters.h`: `Meters` (atomics: pico/RMS in/out, correlación, GR glue/duck/limiter) +
    `LevelMeter`; `AetherfallProcessor::getMeters()`.
  - Flujo final del procesador por trozo: parámetros → modulación → dry (retrasado por latencia) → medidores de
    entrada → InputStage → FXChain → ATMOS CORE → OutputStage (+ medidores de salida).
- **Tests: 71 casos, todos verdes** (+ ducking > 6 dB y recuperación total, limitador ≤ ceiling con +12 dB,
  glue > 10 dB de reducción, tilt/HPF/LPF/width, input HPF/bass mono/gain, procesador nunca supera −0.3 dBFS).

- **F7 – GUI base** (`Source/GUI/`):
  - `LookAndFeel/AetherLookAndFeel` (paleta oscura + acento, knobs de arco vectoriales con modo bipolar y macro,
    toggles tipo píldora, combos, tooltips).
  - `Components/ParamWidgets` (`Knob` con valor al pasar el ratón, doble clic = default, Ctrl/Cmd = fino;
    `ChoiceBox`, `ToggleSwitch`, `Section`, `createParamWidget()`, `layoutGrid()`), `Components/Meters`
    (medidor estéreo con peak hold, GR, correlación; timers solo con la vista visible).
  - `Visualizer/AtmosphereView` (niebla + partículas que reaccionan al nivel de salida, 30 fps, versión básica).
  - `Views/BasicViews` (MAIN: macros, global, quick knobs, mix + medidores; CORE; OUTPUT con GR y medidores),
    `Views/ChainView` (8 slots con selector de módulo, on/mix/solo/parallel, drag & drop para mover, panel de
    parámetros del slot seleccionado, editor de patrón del Trance Gate), `Views/ModView` (placeholder para F8).
  - `PluginEditor`: cabecera con logo, pestañas MAIN/CHAIN/CORE/MOD/OUTPUT y bypass; escalado 75–200 % con
    aspecto fijo; escala y pestaña guardadas en el nodo `UI`.
- **Tests: 72 casos, todos verdes** (+ editor abre/redimensiona/cierra y recuerda la escala). Capturas: test
  oculto `[.gui]`.

## A medias
- Nada a medias.

## Siguiente paso exacto
- **F8 – GUI avanzada**:
  1. `Views/ModView`: editores de LFO (forma, rate/sync/div, fase, retrig, polaridad, smooth; editor de puntos de
     la forma custom → `setLfoShape()`), envolventes, secuenciadores (barras editables → `setSeqSteps()`), chaos,
     macros (lista de destinos/cantidades → `setMacroTarget()`), matriz de 32 slots (fuente, destino, cantidad,
     curva, vía → `setModSlot()`).
  2. Drag & drop de fuentes a knobs (estilo Serum/Vital): las fuentes son `DragAndDropContainer` sources; el
     `Knob` acepta el drop y asigna el primer slot libre de la matriz con cantidad por defecto; anillo de color
     alrededor del knob con la cantidad y el valor modulado en tiempo real (leer el valor efectivo: exponer un
     array atómico de valores efectivos desde `ModulationEngine`).
  3. Undo/Redo (≥ 50 pasos): `juce::UndoManager` en el procesador pasado al APVTS (hoy `nullptr`) y a los writers
     de estado (`setChainOrder`, `setModSlot`... ya aceptan `UndoManager*`), botones en la cabecera + Ctrl/Cmd+Z.
  4. A/B: dos copias del estado completo, botón A/B y "copiar A→B".
  5. Randomize inteligente por sección con intensidad.
  6. Visualizador mejorado (granos como puntos del Granular, decay como niebla) + analizador de espectro del wet
     (FIFO lock-free desde el procesador + FFT en el hilo de mensajes).
  7. Revisar fps (< 60), sin fugas al abrir/cerrar (leak detector en Debug).

## Bugs conocidos / pendientes
- El job de **macOS** del CI se cancela al instante sin logs (probablemente la cuenta no tiene minutos de
  runners macOS para repos privados o hay un límite de gasto). Revisar en GitHub → Settings → Billing, o hacer el
  repo público. Linux/Windows validan igualmente.
- pluginval no se puede ejecutar dentro del contenedor de las sesiones de Claude (bloqueado por permisos);
  se valida en CI.
- Ajustes de sonido pendientes de escucha real (no posible en el contenedor): niveles relativos de los módulos,
  carácter de los color modes, profundidades máximas de modulación, timbre del Drone y de las texturas.
- CPU de la cadena extrema 9.3 % (> 8 % objetivo para preset típico; el típico está en 4.3 %). Candidatos F10:
  trig por muestra en ModFX/Stereo/Drone (usar fasores), Spectral (atan2/sin/cos por bin), Granular (ventanas).
- MIDI CC distintos de CC1 todavía no son fuente (solo mod wheel); añadir "MIDI CC n" configurable si hace falta.
- GUI: el selector de presets de la cabecera llega en F9; la vista MOD es un placeholder hasta F8.
