# AETHERFALL

Procesador FX de atmósferas inmersivas y texturas sintetizadas (VST3 / AU / Standalone),
construido con **JUCE 8** y **CMake**. Pensado sobre todo para **FL Studio** (Windows y macOS),
compatible con cualquier DAW.

> Estado: en desarrollo activo. Ver [`PROGRESS.md`](PROGRESS.md) para el estado actual,
> [`SPEC.md`](SPEC.md) para la especificación completa y [`DECISIONS.md`](DECISIONS.md)
> para las decisiones técnicas.

---

## Requisitos

| | Windows 10/11 x64 | macOS 12+ |
|---|---|---|
| Compilador | Visual Studio 2022 (MSVC, workload "Desktop development with C++") | Xcode 14+ (Command Line Tools) |
| CMake | ≥ 3.22 | ≥ 3.22 |
| Opcional | Ninja | Ninja (`brew install ninja`) |

JUCE (8.0.17) y Catch2 se descargan solos con `FetchContent` la primera vez que se configura.
Si prefieres una copia local de JUCE: clónala en `libs/JUCE` o pasa `-DAETHERFALL_JUCE_DIR=/ruta/a/JUCE`.

Linux (solo para desarrollo/CI) necesita:
`libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev libxcomposite-dev libfreetype-dev libfontconfig1-dev libgl1-mesa-dev`.

## Compilar

### Windows (Visual Studio 2022)

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

### macOS (Universal: Apple Silicon + Intel)

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --parallel
```

### Tests

```bash
ctest --test-dir build -C Release --output-on-failure
```

Opciones de CMake útiles:

| Opción | Por defecto | Qué hace |
|---|---|---|
| `AETHERFALL_BUILD_TESTS` | `ON` | Compila el runner de tests (Catch2) |
| `AETHERFALL_COPY_AFTER_BUILD` | `OFF` | Copia los plugins a las carpetas del sistema tras compilar |
| `AETHERFALL_ENABLE_LTO` | `OFF` | Link-time optimisation |
| `AETHERFALL_JUCE_DIR` | vacío | Ruta a una copia local de JUCE |
| `AETHERFALL_BUILD_CLAP` | `OFF` | Compila también el formato CLAP (clap-juce-extensions) |

Los binarios quedan en `build/Aetherfall_artefacts/Release/{VST3,AU,Standalone}`.

## Instalar

### Windows

Copia la carpeta `AETHERFALL.vst3` a:

```
C:\Program Files\Common Files\VST3\
```

### macOS

```bash
cp -R build/Aetherfall_artefacts/Release/VST3/AETHERFALL.vst3 /Library/Audio/Plug-Ins/VST3/
cp -R build/Aetherfall_artefacts/Release/AU/AETHERFALL.component /Library/Audio/Plug-Ins/Components/
```

Si descargas los binarios del CI (GitHub Actions → artefactos), macOS los marca en cuarentena:

```bash
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/AETHERFALL.vst3
sudo xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/AETHERFALL.component
```

## Usar en FL Studio

1. *Options → Manage plugins → Find plugins* (o "Find installed plugins").
2. AETHERFALL aparece como **efecto**: cárgalo en un slot de un **Mixer Track** (no en el Channel Rack).
3. Funciona en mono→estéreo y estéreo→estéreo. Las colas siguen sonando con el transporte detenido.

## CI y releases

Cada push compila en Windows, macOS (Universal) y Linux, ejecuta los tests unitarios,
valida el VST3 con **pluginval** (nivel 10) y el AU con **auval**, y sube los binarios
como artefactos descargables del workflow.

Para publicar una versión: `git tag v0.1.0 && git push origin v0.1.0`. El workflow `release`
compila Windows y macOS (VST3, AU, CLAP, Standalone; LTO activado) y crea un GitHub Release con
`AETHERFALL-v0.1.0-Windows.zip` y `AETHERFALL-v0.1.0-macOS.zip`.

## Uso rápido

- **MAIN**: las 4 macros (HAZE, DRIFT, GRIT, BLOOM), Dry/Wet, Width, Ducking y Output; tonalidad/escala global
  (usada por Drone y Resonator), color del tanque, Freeze y calidad (Eco/Normal/High).
- **CHAIN**: 8 slots. Elige un módulo en cada slot, arrastra la cabecera de un slot para moverlo. Cada slot tiene
  On, Mix, Solo (S) y Parallel (P: suena en paralelo con el slot anterior).
- **CORE**: todos los parámetros del motor ATMOS CORE.
- **MOD**: LFOs, envolventes, secuenciadores, chaos y macros, matriz de 32 slots. Arrastra un "chip" de fuente
  (barra inferior) sobre cualquier knob (las pestañas se abren al pasar por encima) o usa clic derecho en el knob →
  "Modulate with".
- **OUTPUT**: etapa de entrada, EQ de salida, glue, ducker, clipper/limitador y medidores.
- Cabecera: presets (◀ ▶ y navegador con búsqueda/favoritos), Undo/Redo (Ctrl/Cmd+Z), A/B (clic derecho: copiar
  al otro), RAND (aleatorio inteligente por sección), Bypass.
- Knobs: doble clic = valor por defecto, Ctrl/Cmd + arrastrar = ajuste fino.

## Tests de host (automáticos)

`AetherfallHostTests` carga el **binario VST3 compilado** a través de la API VST3, como lo hace un DAW, y
ejecuta 13 escenarios: escaneo, instanciar y procesar, duplicar la instancia (copia idéntica), cambios de
sample rate / buffer en caliente (22.05–192 kHz, bloques de 1 a 2048), render offline, transporte parado (las
colas siguen), automatización aleatoria de parámetros, bypass del host sin clicks, recarga de proyecto con el
módulo Spectral (latencia de 4096 muestras reportada y señal seca alineada), pista mono, 8 instancias a la vez,
cargar/descargar el plugin y abrir/redimensionar/cerrar la ventana. Se ejecutan con `ctest` y en el CI (en
Windows contra el `.vst3` de Windows).

## QA manual (antes de una release)

Estas pruebas necesitan un DAW real y no se pueden automatizar en el CI:

- [ ] FL Studio 21/2024 (Windows y macOS): *Find plugins*, cargar en un slot del **Mixer**, duplicar la
      instancia, guardar y reabrir el proyecto (preset, cadena, matriz y tamaño de ventana se recuperan).
- [ ] Cambiar sample rate (44.1 → 96 kHz) y tamaño de buffer con el plugin abierto; render offline.
- [ ] Detener el transporte: las colas siguen sonando. LFOs sincronizados caen en el tiempo correcto.
- [ ] Automatizar Dry/Wet, una macro y un parámetro de módulo desde FL; "Browse parameters" muestra nombres cortos.
- [ ] Añadir/quitar el módulo Spectral: FL recompensa la latencia (4096 muestras) y la señal seca sigue alineada.
- [ ] Bypass del host y botón Bypass sin clicks.
- [ ] Ableton Live, Logic Pro (AU), Reaper: cargar, automatizar, guardar/abrir.
- [ ] Escala de la GUI 75 %–200 % en pantallas normales y Retina/HiDPI.

## Licencias

Proyecto de uso personal. JUCE 8 se distribuye bajo AGPLv3 / licencia comercial de JUCE;
para uso personal y no comercial basta con la licencia gratuita.
