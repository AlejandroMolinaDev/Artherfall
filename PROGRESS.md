# PROGRESS — estado del proyecto

> Este archivo es la memoria entre sesiones. Actualizar SIEMPRE antes de cerrar una sesión.

## Estado actual
- **Fase:** F4 – Módulos restantes **terminada** (18/18 módulos) → siguiente: **F5 – Modulación**.
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

## A medias
- Nada a medias.

## Siguiente paso exacto
- **F5 – Modulación** en `Source/DSP/Modulation/` y `Source/DSP/Analysis/`:
  1. `ModSources.h`: enum de fuentes (LFO1–8, Env1–4, Seq1–2, Chaos1–2, Macro1–4, Analyzer: RMS, transient,
     pitch, centroid; MIDI: note, velocity, modwheel, CC) — los IDs de fuente se guardan en el estado.
  2. `LFO` (formas sine/tri/saw/square/S&H/smooth random/custom, sync con `TempoPhase`, fase, retrigger
     Free/Transport/Transient/MIDI, polaridad, smooth), `EnvFollower` (input/wet), `StepSeq` (pasos en el nodo
     `SEQ` del estado), `Chaos` (Lorenz / drunk walk).
  3. `InputAnalyzer` (RMS, detector de transitorios, `PitchTracker`, centroide espectral) → rellena
     `ModContext::analysis` (el Comb ya lo usa en modo Track).
  4. `ModMatrix`: 32 slots (fuente, destino = cualquier `PID` con `isModulatable()`, cantidad = `modN_amt`, curva,
     vía) en el nodo `MODMATRIX` del estado + snapshot lock-free (triple buffer) para el audio thread. En
     `processChunk` sustituir la copia `effective = params` por: valores base normalizados + Σ modulaciones,
     desnormalizados con `makeRange()` (precalcular los rangos en un array).
  5. `MacroMap`: 4 macros × 8 destinos (rango min/max por destino) en el nodo `MACROS`; mapeo por defecto de la
     spec (HAZE/DRIFT/GRIT/BLOOM). `glob_moddepth` escala las fuentes moduladoras.
  6. MIDI: leer el `MidiBuffer` en `processBlock` (nota, velocity, mod wheel, CC) para las fuentes MIDI y el
     retrigger de LFOs.
  7. Tests: LFO sincronizado a 120 BPM cae en la fase correcta, matriz modula un destino dentro de rango,
     macros mapean, estado de la matriz se guarda/recupera.

## Bugs conocidos / pendientes
- El job de **macOS** del CI se cancela al instante sin logs (probablemente la cuenta no tiene minutos de
  runners macOS para repos privados o hay un límite de gasto). Revisar en GitHub → Settings → Billing, o hacer el
  repo público. Linux/Windows validan igualmente.
- pluginval no se puede ejecutar dentro del contenedor de las sesiones de Claude (bloqueado por permisos);
  se valida en CI.
- Ajustes de sonido pendientes de escucha real (no posible en el contenedor): niveles relativos de los módulos,
  carácter de los color modes, profundidades máximas de modulación, timbre del Drone y de las texturas.
- CPU de la cadena extrema 9.3 % (> 8 % objetivo para preset típico; el típico está en 3.7 %). Candidatos F10:
  trig por muestra en ModFX/Stereo/Drone (usar fasores), Spectral (atan2/sin/cos por bin), Granular (ventanas).
