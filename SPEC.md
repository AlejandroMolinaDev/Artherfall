# AETHERFALL — Especificación técnica (SPEC.md)

> Procesador FX de atmósferas inmersivas y texturas sintetizadas.
> Formato principal: VST3 para FL Studio (Windows + macOS). Compatible con cualquier DAW.
> Uso: personal. Proyecto multi-sesión de larga duración.

---

## 0. Protocolo de trabajo multi-sesión (LEER PRIMERO)

Este proyecto es **grande a propósito** y NO se espera terminarlo en una sola sesión. Se construye de forma iterativa durante varias sesiones y varios días, sin límite de sesiones.

**Reglas obligatorias para Claude en cada sesión:**

1. **Al iniciar:** leer `SPEC.md`, `PROGRESS.md` y `DECISIONS.md` (si existen). Revisar `git log --oneline -20`. Continuar exactamente donde quedó la sesión anterior.
2. **Trabajar en fases largas:** escribir mucho código por fase. No detenerse a pedir confirmación en cosas que esta spec ya define. Solo preguntar ante decisiones irreversibles o contradicciones.
3. **Commits frecuentes y pequeños** (al menos uno por módulo terminado), con mensajes claros (`feat(dsp): granular cloud engine`). Push al terminar cada bloque.
4. **El proyecto debe compilar siempre** al final de cada commit. Si algo queda a medias, dejarlo detrás de un flag o stub que compile.
5. **Antes de que se acabe el contexto o el crédito**, actualizar `PROGRESS.md` con:
   - Fase y tarea actuales, qué quedó terminado, qué quedó a medias (archivo + función).
   - Siguiente paso exacto ("Siguiente: implementar `SpectralFreeze::process()` en `Source/DSP/SpectralFreeze.cpp`").
   - Bugs conocidos y pendientes.
   Hacer commit + push de ese archivo. Esto es lo más importante: la siguiente sesión depende de él.
6. Registrar decisiones técnicas relevantes en `DECISIONS.md` (fecha + decisión + motivo).
7. Si una sesión se corta sin actualizar `PROGRESS.md`, la siguiente reconstruye el estado con `git log` y el código, y lo actualiza antes de seguir.

---

## 1. Visión del producto

**AETHERFALL** es un FX de inserción que convierte cualquier fuente (pads, synths, guitarras, voces, plucks, incluso drums) en **atmósferas ambientales enormes, oscuras y con mucha presencia**, con un carácter **más sintetizado y más rudo** que una reverb tradicional.

### Referencia estética (de resultado, no de copia)
- La atmósfera de la outro de *"Blackout"* y la intro *"Bad Trip"* (álbum *Microdosis*) de Mora: sintetizadores ambientales amplios, envolventes, densos, con sensación de "flotar" pero con peso en el centro.
- Dirección extra propia: más sintética y agresiva — saturación, bitcrush suave, movimiento granular, filtros resonantes que respiran, capas de shimmer y drones que se sienten "vivos".

### Palabras clave del sonido
`wide` · `dark` · `lush` · `hazy` · `granular` · `drifting` · `saturated` · `breathing` · `massive tails` · `cinematic but urban`

### Principios
- Un solo plugin capaz de ir de "colchón sutil detrás de la voz" a "muro ambiental total".
- Todo modulable. Mucho movimiento, pero musical (sincronizado al tempo).
- Interfaz limpia estilo Valhalla en la vista principal; profundidad en vistas avanzadas.
- CPU razonable: objetivo < 8 % de un núcleo a 48 kHz / buffer 256 con preset típico.

---

## 2. Plataforma y stack técnico

| Aspecto | Decisión |
|---|---|
| Framework | **JUCE 8** (última estable) |
| Build | **CMake** (≥ 3.22), JUCE vía `FetchContent` o submódulo |
| Lenguaje | C++20 |
| Formatos | **VST3** (prioridad, Win + Mac), **AU** (Mac, para Logic), **Standalone** (para pruebas). CLAP opcional vía `clap-juce-extensions` (fase final). AAX fuera de alcance. |
| SO | Windows 10/11 x64, macOS 12+ Universal (arm64 + x86_64) |
| DAW objetivo | **FL Studio 21/2024+** (prioridad), luego Ableton Live, Logic Pro, Reaper |
| Validación | **pluginval** (nivel 10), auval en Mac |
| CI | GitHub Actions: build Win + Mac en cada push, artefactos descargables |
| Tests | Catch2 o JUCE UnitTest para DSP (sin NaN/denormals, nivel de salida, reset) |

### Notas específicas de FL Studio
- Se usa como **efecto de inserción en el Mixer** (slots del Mixer Track). El Channel Rack es para generadores/instrumentos; este plugin es FX, así que va en el Mixer.
- Bus layouts soportados: mono→stereo, stereo→stereo.
- Reportar **latencia** correctamente (`setLatencySamples`) si algún módulo la añade (ej. espectral).
- Manejar bien: cambios de sample rate/buffer en caliente, transporte detenido (las colas deben seguir sonando), tempo vía `AudioPlayHead`.
- Nombre de parámetros cortos y legibles (FL los muestra en "Browse parameters" y para automatización).
- Ruta de instalación: `C:\Program Files\Common Files\VST3\` y `/Library/Audio/Plug-Ins/VST3/`.
- Probar con "Plugin manager → Find plugins" de FL.

---

## 3. Arquitectura general de señal

```
INPUT
  │
  ├─► [Input Analyzer] ──────────────► (fuentes de modulación: envelope, transients, pitch, spectral centroid)
  │
  ▼
[Input Stage]  gain · HPF/LPF · drive previo · mono-maker de graves
  │
  ▼
[Ducker / Sidechain interno]  (aplicado al final sobre el wet)
  │
  ▼
┌──────────────── FX CHAIN (8 slots, reordenables por drag & drop) ────────────────┐
│ Slot 1 … Slot 8: cualquiera de los módulos de la sección 4                       │
│ Cada slot: on/off · mix · solo · posición serie/paralelo                         │
└──────────────────────────────────────────────────────────────────────────────────┘
  │
  ▼
[ATMOS CORE] — motor principal siempre presente (reverb + shimmer + bloom)
  │
  ▼
[Output Stage]  tilt EQ · width · glue compressor · soft clipper · ducking · dry/wet · output gain
  │
  ▼
OUTPUT
```

- Procesamiento interno a **64-bit double** en reverbs/feedback largos; float en el resto.
- Oversampling opcional (1x/2x/4x) solo en módulos de distorsión.
- Todo parámetro con **smoothing** (sin zipper noise).
- Protección: DC blocker, limitador de seguridad en loops de feedback, limpieza de denormals (`juce::ScopedNoDenormals`).

---

## 4. Módulos DSP

### 4.1 ATMOS CORE (motor principal, siempre activo)
Reverb algorítmica tipo FDN (16 líneas) con difusión allpass en serie.
- **Size** (0.1–1.0) · **Decay** (0.2 s – 60 s + modo **Infinite/Freeze**)
- **Pre-delay** (0–500 ms, sync opcional)
- **Diffusion** early/late
- **Modulation** depth/rate interna (chorus en las líneas)
- **Damping** HF/LF independientes
- **Shimmer**: pitch shifter en el loop de feedback (+12, +7, +5, −12, +19, +24; mezcla de dos intervalos)
- **Bloom**: el ataque de la cola crece lentamente (envolvente de swell en la entrada del tanque)
- **Color modes**: `Clean`, `Dark`, `Vintage 80s` (sample rate reducido emulado), `Lo-Fi`, `Void` (oscuro y gigante)

### 4.2 Módulos para los 8 slots
1. **Granular Cloud** — buffer circular de 8 s, hasta 64 granos; tamaño, densidad, spray de posición, pitch (+ aleatorio), reversa %, ventana (Hann/Tukey/Triangular), freeze del buffer.
2. **Spectral Freeze / Smear** — FFT (2048/4096), congelar espectro, smear temporal, blur, "spectral shimmer" (desplazar bins). Reporta latencia.
3. **Shimmer Delay** — delay estéreo con pitch shift en el feedback, ping-pong, filtros en el loop.
4. **Tape Delay** — wow & flutter, saturación de cinta, degradación por repetición, sync al tempo (1/32 a 4 compases, tresillos y con puntillo).
5. **Multi-Tap Diffuse Delay** — 8 taps con difusión progresiva (de eco a nube).
6. **Drone Generator** — resonador que extrae la tonalidad de la entrada (pitch tracking) y genera un drone sostenido (osciladores sine/saw filtrados + ruido) siguiendo la nota detectada. Clave para el sonido "sintetizado".
7. **Resonator Bank** — 4–8 resonadores afinados (modo acorde: menor, menor9, sus2, quinta, etc. con tonalidad seleccionable).
8. **Comb / Karplus Smear** — filtros comb modulados para metales y texturas.
9. **Filter Sweep** — filtro multimodo (LP/HP/BP/Notch/Formant), 12/24 dB, resonancia autooscilante, estilo SVF/ladder.
10. **Phaser / Flanger / Chorus** — tres modos, hasta 12 etapas, ensemble estéreo amplio.
11. **Saturator** — modos Tape, Tube, Diode, Fold (wavefolder), Fuzz; oversampling.
12. **Bitcrush / Decimate** — bits y sample rate con suavizado (modo "rudo pero musical").
13. **Ring Mod / Freq Shifter** — para texturas inarmónicas y movimiento.
14. **Stereo Engine** — Haas, M/S width, auto-pan, rotación ("orbita"), bass mono.
15. **Gate / Trance Gate** — patrón de 16 pasos sincronizado con swing y suavizado.
16. **Reverse Swell** — captura en vivo y reproduce en reversa (efecto "succión" previa).
17. **Texture Layer** — capa de ruido interna (vinilo, cinta, viento, lluvia sintética, hum) generada proceduralmente (sin samples externos), con envelope follower.
18. **Lo-Fi Tape Machine** — saturación + compresión + pérdida de agudos + wobble global.

---

## 5. Sistema de modulación

Objetivo: **muchísimas modulaciones**, todas asignables.

### Fuentes
- **8 LFOs**: formas sine/tri/saw/square/S&H/smooth random/custom (editor de puntos), sync o Hz, fase, retrigger, unipolar/bipolar.
- **4 Envelope Followers**: attack/release, sobre la entrada o sobre la señal wet.
- **2 Step Sequencers** (16–32 pasos, sync, glide).
- **2 Random/Chaos** (Lorenz / drunk walk lento para movimiento orgánico).
- **Input Analyzer**: transient detector, nivel RMS, pitch detectado, centroide espectral (brillo).
- **4 Macros** (ver sección 6).
- **MIDI** (opcional): nota, velocity, mod wheel, CC — FL permite enrutar MIDI a FX.

### Matriz
- **Mínimo 32 slots** de modulación: fuente → destino, cantidad (bipolar), curva, vía (otra fuente como multiplicador).
- Cualquier parámetro automatizable puede ser destino.
- Drag & drop desde la fuente hacia el knob (estilo Serum/Vital), con anillo de color indicando cantidad.
- Visualización en tiempo real del valor modulado en cada knob.

---

## 6. Macros (vista principal)

4 macros grandes que controlan muchos parámetros a la vez (mapeables por preset):

| Macro | Función por defecto |
|---|---|
| **HAZE** | Tamaño + decay + difusión + mezcla granular |
| **DRIFT** | Cantidad de modulación global (LFOs, wobble, pitch spray) |
| **GRIT** | Saturación + bitcrush + resonancia del filtro |
| **BLOOM** | Shimmer + drone + swell |

Más: **Dry/Wet**, **Width**, **Ducking**, **Output**.

---

## 7. Interfaz gráfica

- Estilo **limpio tipo Valhalla** en la vista principal; oscuro, con un color de acento por preset/categoría.
- **Redimensionable** (75 %–200 %), vectorial (sin bitmaps borrosos), soporte HiDPI/Retina.
- Tamaño base: 1000 × 640.

### Vistas (pestañas)
1. **MAIN** — 4 macros grandes, Dry/Wet, Width, Output, selector de preset, visualizador central (partículas/nube que reacciona al audio — granos como puntos, decay como niebla).
2. **CHAIN** — los 8 slots en fila, drag & drop para reordenar, cada slot expandible con sus parámetros.
3. **CORE** — parámetros completos de ATMOS CORE.
4. **MOD** — LFOs, envelopes, secuenciadores, matriz de modulación.
5. **OUTPUT** — EQ de salida, glue comp, ducker, limitador, medidores.

### Detalles
- Medidores de entrada/salida, correlación estéreo, analizador de espectro del wet.
- Tooltips en todos los controles.
- Doble clic = valor por defecto; Ctrl/Cmd + arrastre = ajuste fino.
- Undo/Redo (mínimo 50 pasos).
- Botón **A/B** para comparar ajustes.
- Botón **Randomize** inteligente (por sección, con "intensidad").
- Renderizado del visualizador con OpenGL opcional (fallback a software), máximo 60 fps, que no consuma CPU con la ventana cerrada.

---

## 8. Presets

- Formato: XML/JSON propio en carpeta de usuario (`Documents/AETHERFALL/Presets`), más estado completo vía `getStateInformation`.
- Navegador con categorías, favoritos, búsqueda, anterior/siguiente.
- **Mínimo 60 presets de fábrica**, en categorías:
  - **Outro Skies** — colas largas y oscuras para finales de canción
  - **Intro Haze** — swells y drones para intros ambientales
  - **Vocal Atmos** — colchones detrás de la voz (con ducking)
  - **Synth Walls** — muros densos y saturados
  - **Granular Dreams** — texturas en movimiento
  - **Dark & Rude** — sintético, sucio, agresivo
  - **Frozen** — freeze/infinite pads
  - **Rhythmic Space** — gates y delays sincronizados
  - **Subtle** — mezcla discreta para uso diario
- Presets diseñados en código como datos (tabla de valores), documentados en `presets/README.md`.

---

## 9. Estructura del repositorio

```
AETHERFALL/
├─ CMakeLists.txt
├─ SPEC.md            ← este documento
├─ PROGRESS.md        ← estado actual (se actualiza cada sesión)
├─ DECISIONS.md       ← decisiones técnicas
├─ README.md          ← cómo compilar e instalar (Win/Mac)
├─ .github/workflows/build.yml
├─ libs/              (JUCE, etc.)
├─ Source/
│  ├─ PluginProcessor.{h,cpp}
│  ├─ PluginEditor.{h,cpp}
│  ├─ Parameters/     (layout APVTS, IDs, rangos)
│  ├─ DSP/
│  │  ├─ Core/        (AtmosCore, FDN, Diffuser, PitchShifter)
│  │  ├─ Modules/     (un archivo por módulo de la sección 4.2)
│  │  ├─ Chain/       (FXChain, Slot, interfaz común IModule)
│  │  ├─ Modulation/  (LFO, EnvFollower, StepSeq, Chaos, ModMatrix, Macros)
│  │  ├─ Analysis/    (InputAnalyzer, PitchTracker, TransientDetector)
│  │  └─ Utils/       (Smoothers, DCBlocker, Oversampler, DelayLine, Interp)
│  ├─ GUI/
│  │  ├─ LookAndFeel/
│  │  ├─ Components/  (Knob, MacroKnob, SlotView, ModSourceDrag, Meters)
│  │  ├─ Views/       (MainView, ChainView, CoreView, ModView, OutputView)
│  │  └─ Visualizer/
│  └─ Presets/        (PresetManager, FactoryPresets)
├─ tests/
└─ presets/
```

Todos los módulos de efecto implementan una interfaz común:
```cpp
struct IModule {
    virtual void prepare (double sampleRate, int maxBlock, int numChannels) = 0;
    virtual void reset() = 0;
    virtual void process (juce::AudioBuffer<float>&, const ModContext&) = 0;
    virtual int  getLatencySamples() const { return 0; }
    virtual ~IModule() = default;
};
```

---

## 10. Fases de desarrollo

Cada fase es larga. Al terminar cada una: compilar, probar en Standalone, pasar pluginval, commit + push, actualizar `PROGRESS.md`.

| Fase | Contenido | Criterio de "terminado" |
|---|---|---|
| **F0 – Base** | Repo, CMake, JUCE, CI Win+Mac, plugin vacío que pasa audio, `PROGRESS.md`/`DECISIONS.md` | Carga en FL Studio sin errores; CI verde |
| **F1 – Parámetros y utilidades** | APVTS completo, smoothers, delay lines, DC blocker, oversampler, interfaz `IModule` | Tests de utilidades pasan |
| **F2 – ATMOS CORE** | FDN, difusión, shimmer, bloom, freeze, color modes | Colas largas estables, sin clipping ni NaN; infinite funciona |
| **F3 – Chain + primeros módulos** | Sistema de 8 slots reordenables + Granular, Tape Delay, Filter, Saturator, Stereo | Reordenar en tiempo real sin clicks |
| **F4 – Módulos restantes** | Spectral, Shimmer Delay, Multi-Tap, Drone, Resonator, Comb, Phaser/Flanger/Chorus, Bitcrush, Ring/Freq, Gate, Reverse Swell, Texture, Lo-Fi | Todos los 18 módulos funcionales |
| **F5 – Modulación** | LFOs, envelopes, seqs, chaos, Input Analyzer, matriz 32 slots, macros | Cualquier parámetro modulable; sync al tempo de FL correcto |
| **F6 – Output Stage** | Tilt EQ, glue comp, ducker, soft clip, limitador | Ducking audible y musical |
| **F7 – GUI base** | LookAndFeel, vistas MAIN/CHAIN/CORE/OUTPUT, redimensionado | Todo controlable desde la UI |
| **F8 – GUI avanzada** | Vista MOD con drag & drop, anillos de modulación, visualizador, medidores, undo/redo, A/B | UI fluida, < 60 fps, sin fugas |
| **F9 – Presets** | PresetManager, navegador, 60+ presets de fábrica | Presets cargan/guardan, estado se recupera al reabrir proyecto FL |
| **F10 – Optimización** | Perfilado de CPU, SIMD donde convenga, calidad eco/normal/alta | Meta de CPU cumplida |
| **F11 – QA y entrega** | pluginval nivel 10, pruebas en FL Studio (Win/Mac), Ableton, Logic, Reaper; README de instalación; CLAP opcional | Sin crashes, binarios en GitHub Releases |

---

## 11. Requisitos de calidad

- Cero crashes al: cargar/descargar, cambiar sample rate, cambiar buffer, render offline, duplicar el plugin en FL.
- Sin NaN/Inf ni denormals (tests automáticos con ruido, silencio, impulsos, DC).
- Estado completo guardado con el proyecto (incluida la matriz de modulación y el orden de la cadena).
- Bypass sin clicks; las colas siguen al detener transporte.
- Thread-safety: nada de allocations ni locks en el audio thread.
- Código comentado en las partes DSP no obvias (para fines de aprendizaje).

---

## 12. Fuera de alcance

- AAX / Pro Tools.
- Copiar código, presets, gráficos o nombres de productos comerciales. Todo el DSP y diseño es original.
- Samples externos (todo procedural).
- Licenciamiento/DRM (es uso personal).
