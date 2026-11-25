# PROGRESS — estado del proyecto

> Este archivo es la memoria entre sesiones. Actualizar SIEMPRE antes de cerrar una sesión.

## Estado actual
- **Fase:** F2 – ATMOS CORE **terminada** → siguiente: **F3 – Chain + primeros módulos**.
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

## A medias
- Nada a medias. (Input Stage, chain y Output Stage aún no existen; el flujo actual es
  entrada → ATMOS CORE → dry/wet provisional.)

## Siguiente paso exacto
- **F3:** crear `Source/DSP/Chain/FXChain.{h,cpp}`:
  1. Estado del orden de la cadena en el ValueTree (`CHAIN` node, propiedad `order` = ids de `moduleInfos`
     separados por comas, máx. 8, sin duplicados) + espejo lock-free para el audio thread (`std::atomic<uint64_t>`,
     8 bytes = 8 slots con el `ModuleType`).
  2. `FXChain::process()`: recorre los slots; por slot aplica on/off (crossfade suave), mix (dry/wet), solo
     (si algún módulo tiene solo, solo suenan los que lo tienen) y parallel (un slot paralelo recibe la misma
     entrada que el slot serie anterior; las salidas del grupo se promedian).
  3. Reordenar sin clicks: al cambiar el orden, fundido de ~10 ms de la salida de la cadena hacia su entrada,
     cambio de orden en el punto de silencio relativo, fundido de vuelta.
  4. `Source/DSP/Modules/ModuleFactory` + primeros módulos: Granular Cloud, Tape Delay, Filter Sweep,
     Saturator, Stereo Engine (archivos `Source/DSP/Modules/<Nombre>.{h,cpp}`).
  5. Insertar la cadena en `processChunk` antes de `core.process`. Tests de la cadena (reordenar sin clicks,
     solo, parallel) y de cada módulo (sin NaN, niveles).

## Bugs conocidos / pendientes
- El job de **macOS** del CI se cancela al instante sin logs (probablemente la cuenta no tiene minutos de
  runners macOS para repos privados o hay un límite de gasto). Revisar en GitHub → Settings → Billing, o hacer el
  repo público. Linux/Windows validan igualmente.
- pluginval no se puede ejecutar dentro del contenedor de las sesiones de Claude (bloqueado por permisos);
  se valida en CI.
- Ajustes de sonido pendientes de escucha real (no posible en el contenedor): nivel con Decay muy corto + Size
  grande queda ~ −11 dB; carácter de cada color mode; profundidad máxima de modulación.
