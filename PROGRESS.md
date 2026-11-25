# PROGRESS — estado del proyecto

> Este archivo es la memoria entre sesiones. Actualizar SIEMPRE antes de cerrar una sesión.

## Estado actual
- **Fase:** F3 – Chain + primeros módulos **terminada** → siguiente: **F4 – Módulos restantes**.
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

## A medias
- Nada a medias.

## Siguiente paso exacto
- **F4 – módulos restantes**, en `Source/DSP/Modules/<Nombre>.{h,cpp}`, registrando cada uno en
  `ModuleFactory.cpp` (`createModule` + lista `implemented` de `isModuleImplemented`). Orden sugerido (de más
  simple a más complejo): `Bitcrush`, `RingFreq`, `ModFX` (phaser/flanger/chorus), `ShimmerDelay` (reutiliza
  `dsp::PitchShifter`), `MultiTap`, `Comb`, `Resonator`, `Gate` (patrón de 16 pasos: guardar como propiedad
  `pattern` (uint16) en un nodo `GATE` del estado + espejo atómico, igual que la cadena), `ReverseSwell`,
  `Texture`, `LoFi`, `Drone` (necesita pitch tracking: crear `Source/DSP/Analysis/PitchTracker`),
  `Spectral` (FFT 2048/4096, reporta latencia: `FXChain::getLatencySamples()` → `setLatencySamples()` en el
  procesador + retrasar la señal seca en igual medida).
- Cada módulo: añadir test específico en `tests/ModuleTests.cpp` (el test de estrés ya cubre los 18).

## Bugs conocidos / pendientes
- El job de **macOS** del CI se cancela al instante sin logs (probablemente la cuenta no tiene minutos de
  runners macOS para repos privados o hay un límite de gasto). Revisar en GitHub → Settings → Billing, o hacer el
  repo público. Linux/Windows validan igualmente.
- pluginval no se puede ejecutar dentro del contenedor de las sesiones de Claude (bloqueado por permisos);
  se valida en CI.
- Ajustes de sonido pendientes de escucha real (no posible en el contenedor): nivel con Decay muy corto + Size
  grande queda ~ −11 dB; carácter de cada color mode; profundidad máxima de modulación; balance de niveles entre
  módulos.
- La latencia del plugin es 0 por ahora; cuando exista el módulo Spectral habrá que compensar la señal seca.
