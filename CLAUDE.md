# CLAUDE.md — instrucciones para Claude Code en este repo

Este archivo se carga automáticamente al empezar cada sesión. **Sigue este protocolo sin pedir confirmación.**

## Al iniciar una sesión
1. Lee `SPEC.md` (fuente de verdad), `PROGRESS.md` (estado y **siguiente paso exacto**) y `DECISIONS.md`.
2. Revisa `git log --oneline -20` y `git status`.
3. **Continúa exactamente con la sección "Siguiente paso exacto" / "En curso" de `PROGRESS.md`.** Si hay una
   tarea marcada como "En curso", termínala antes de empezar otra.
4. Si la sesión anterior se cortó sin actualizar `PROGRESS.md`, reconstruye el estado con `git log` y el código y
   actualízalo antes de seguir.

## Mientras trabajas
- Commits pequeños y frecuentes (`feat(dsp): ...`, `fix(gui): ...`, `test(...): ...`), push a la rama de trabajo.
- El proyecto debe compilar y los tests deben pasar en cada commit.
- Antes de que se acabe el contexto: actualiza `PROGRESS.md` (terminado / a medias con archivo y función /
  siguiente paso exacto / bugs) y haz commit + push. Registra decisiones en `DECISIONS.md`.

## Comandos útiles
```bash
# Configurar y compilar (Linux, deps en ~/.cache para no re-descargar JUCE/Catch2)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_BASE_DIR=$HOME/.cache/aetherfall-deps
cmake --build build -j4

# Todos los tests (unitarios + host VST3)
ctest --test-dir build --output-on-failure
./build/tests/AetherfallTests_artefacts/Release/AetherfallTests            # unitarios
./build/tests/AetherfallHostTests_artefacts/Release/AetherfallHostTests    # escenarios de DAW contra el .vst3

# Ocultos: benchmarks y capturas de la GUI
./build/tests/AetherfallTests_artefacts/Release/AetherfallTests "[.bench]"
AETHERFALL_SNAPSHOT_DIR=/tmp/snaps ./build/tests/AetherfallTests_artefacts/Release/AetherfallTests "[.gui]"
```
- Dependencias Linux: ver README. Para tests con ventanas reales: `apt-get install xvfb openbox` y lanzar
  `Xvfb :99 & DISPLAY=:99 openbox &` antes del comando (sin gestor de ventanas JUCE falla en X11).
- pluginval no se puede ejecutar en el contenedor de Claude (permisos): se valida en GitHub Actions.
- El runner de macOS del CI no arranca en esta cuenta (billing); Linux y Windows sí.
