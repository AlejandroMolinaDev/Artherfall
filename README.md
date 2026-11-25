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

## CI

Cada push compila en Windows, macOS (Universal) y Linux, ejecuta los tests unitarios,
valida el VST3 con **pluginval** (nivel 10) y el AU con **auval**, y sube los binarios
como artefactos descargables del workflow.

## Licencias

Proyecto de uso personal. JUCE 8 se distribuye bajo AGPLv3 / licencia comercial de JUCE;
para uso personal y no comercial basta con la licencia gratuita.
