# PROGRESS — estado del proyecto

> Este archivo es la memoria entre sesiones. Actualizar SIEMPRE antes de cerrar una sesión.

## Estado actual
- **Fase:** F0 – Base (en curso)

## Terminado
- F0: CMake + JUCE 8.0.17 (FetchContent) + Catch2, plugin VST3/AU/Standalone que pasa audio
  (mono→estéreo / estéreo→estéreo), editor provisional, CI (Win/Mac/Linux + pluginval + auval), README.

## A medias
- —

## Siguiente paso exacto
- F1: tabla completa de parámetros (APVTS) en `Source/Parameters/`, utilidades DSP en `Source/DSP/Utils/`,
  interfaz `IModule` + `ModContext` en `Source/DSP/Chain/`, tests de utilidades.

## Bugs conocidos / pendientes
- pluginval no se puede ejecutar dentro del contenedor de desarrollo; revisar el resultado en GitHub Actions.
