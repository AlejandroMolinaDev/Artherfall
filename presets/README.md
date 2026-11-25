# AETHERFALL — presets de fábrica

Los presets de fábrica se definen **en código como datos** en
[`Source/Presets/FactoryPresets.cpp`](../Source/Presets/FactoryPresets.cpp): cada preset lista solo los
parámetros que difieren de los valores por defecto (`ParamList.h`), el orden de la cadena, las rutas de
modulación y, si hace falta, el patrón del Trance Gate. Todo lo demás (incluidos los mapas de macros por
defecto HAZE / DRIFT / GRIT / BLOOM) vuelve al estado inicial al cargar un preset.

Los presets de usuario se guardan como XML (`*.afpreset`) en `Documents/AETHERFALL/Presets`; los favoritos en
`Documents/AETHERFALL/Presets/favourites.xml`. El nombre y la categoría del preset cargado viajan con el
proyecto de la DAW.

Total: **63 presets** (7 por categoría). Las 4 macros funcionan en todos.

## Outro Skies — colas largas y oscuras para finales
| Preset | Idea |
|---|---|
| Blackout Skies | Tanque enorme y oscuro (Dark), bloom y un poco de shimmer; LFO lento abre y cierra los agudos. |
| Last Light | Cola de 25 s con shimmer +12/+7 que respira y un Shimmer Delay de fondo. |
| Fading City | Color Vintage 80s + Tape Delay gastado + Lo-Fi: final nostálgico. |
| Void Horizon | Modo Void (gigante y oscuro), 30 s, tilt oscuro. |
| Ember Outro | Saturación de válvula + filtro paso bajo que se mueve lentamente. |
| Tail Spin | Chorus amplio y órbita estéreo sobre una cola larga. |
| Midnight Exit | Drone en quintas + vinilo suave que sigue a la entrada. |

## Intro Haze — swells y drones para intros
| Preset | Idea |
|---|---|
| Bad Trip Intro | Reverse Swell + nube granular en octavas + drone menor 9: neblina sintética densa. |
| Morning Fog | Granular lento y filtrado; la posición de los granos deriva con un LFO. |
| Swell Theory | Bloom al máximo + Reverse Swell de 1.5 s: todo entra "aspirado". |
| Rising Drone | Drone en octavas con glide largo y filtro que se abre muy despacio. |
| Hazy Dawn | Vintage 80s + cinta Lo-Fi con wobble + chorus. |
| Cathedral Mist | Difusión al máximo, tamaño enorme, bloom y shimmer suave. |
| Slow Ignition | Spectral smear/blur con fases aleatorias: lavado espectral. |

## Vocal Atmos — colchones detrás de la voz (con ducking)
| Preset | Idea |
|---|---|
| Vocal Cushion | Room medio con pre-delay y ducking: la cola aparece entre frases. |
| Mora Halo | Halo con shimmer +12 y eco con shimmer en 1/8 con puntillo, HPF a 200 Hz. |
| Whisper Room | Sala pequeña oscura, discreta. |
| Echo Bloom | Tape Delay a negras + bloom, ducking fuerte. |
| Silk Plate | Placa brillante y modulada. |
| Ghost Double | Chorus + multi-tap difuso: doble fantasma alrededor de la voz. |
| Hook Air | Aire brillante (tilt +2 dB) con un toque de shimmer. |

## Synth Walls — muros densos y saturados
| Preset | Idea |
|---|---|
| Concrete Wall | Saturación de cinta + filtro + glue compressor. |
| Neon Wall | Phaser de 8 etapas + válvula + shimmer. |
| Supersaw Fog | Drone en sierra (acorde menor) desafinado y saturado. |
| Pressure Front | Void + saturación de diodo + glue. |
| Analog Monolith | Filtro ladder resonante sincronizado a 2 compases + Vintage 80s. |
| Stack Of Fifths | Banco de resonadores en quintas sobre la tonalidad global. |
| Wide Screen | Estéreo extremo (Haas + width 180 %) con graves en mono. |

## Granular Dreams — texturas en movimiento
| Preset | Idea |
|---|---|
| Cloud Seeding | Nube granular con pitch aleatorio en octavas y quintas, reversas. |
| Glass Particles | Granos cortos una octava arriba, ventana triangular, shimmer. |
| Reverse Snow | 80 % de granos al revés, spray amplio. |
| Time Smear | Granos largos con feedback + smear espectral. |
| Octave Rain | Granos en octavas + lluvia procedural. |
| Drifting Grains | Posición y pitch de los granos derivan con LFOs (smooth random). |
| Grain Field | Campo denso con feedback que se auto-espesa. |

## Dark & Rude — sintético, sucio, agresivo
| Preset | Idea |
|---|---|
| Rude Boy Room | Bitcrush + fuzz en una sala Lo-Fi. |
| Broken Speaker | Diodo + paso banda resonante + bitcrush. |
| Metal Mouth | Comb metálico con smear + ring modulator. |
| Sub Pressure | Válvula + ladder grave resonante + Void + glue. |
| Digital Ghetto | Bitcrush con jitter + frequency shifter con feedback. |
| Grit Engine | Wavefolder + tape delay sobresaturado. |
| Noise Floor | Hiss de cinta que sigue a la entrada + Lo-Fi comprimido. |

## Frozen — pads infinitos (activa FREEZE para congelar)
| Preset | Idea |
|---|---|
| Glacier | 60 s, tamaño máximo, oscuro y modulado. |
| Ice Cathedral | 50 s con shimmer +12/+19. |
| Spectral Ice | Spectral smear casi congelado + cola de 40 s. |
| Permafrost | Void + drone sus2 + bloom. |
| Crystal Lattice | Resonadores menor 9 brillantes + shimmer. |
| Still Water | Cola larga quieta + granos largos y lentos. |
| Frozen Breath | Cola de 55 s que respira (LFOs muy lentos en damping y tamaño). |

## Rhythmic Space — gates y delays sincronizados
| Preset | Idea |
|---|---|
| Trance Gate Room | Trance gate a semicorcheas sobre una cola grande. |
| Dotted Echoes | Tape delay a corcheas con puntillo. |
| Pulse Field | Gate a corcheas con swing (patrón 1-3-5-7…) + shimmer delay a negras. |
| Ping Pong City | Delay ping-pong a corcheas con puntillo. |
| Stutter Bloom | Gate a fusas (patrón 2+2) + Reverse Swell a blancas. |
| Sidechain Haze | Ducking profundo + multi-tap a 1 compás. |
| Tape Rhythm | Multi-tap de 6 taps acelerando a lo largo de 1 compás. |

## Subtle — mezcla discreta para uso diario
| Preset | Idea |
|---|---|
| Just Air | Aire corto y limpio al 15 %. |
| Small Glow | Brillo corto con algo de bloom y HPF. |
| Daily Plate | Placa difusa de 2 s. |
| Soft Width | Ensanchado suave (Haas + M/S) con graves en mono. |
| Warm Tail | Cola Vintage 80s un poco oscura. |
| Ghost Shimmer | Un hilo de shimmer. |
| Gentle Drift | Cola corta modulada + chorus ligero. |

## Añadir presets
1. Añade una entrada a la tabla de `factoryPresets()` (valores en unidades reales: segundos, Hz, 0–1 para %,
   índices para las listas de `ChoiceLists.h`).
2. Ejecuta los tests (`ctest`): comprueban rangos, nombres únicos, ≥ 5 por categoría y que cada preset procesa
   audio sin NaN y por debajo del ceiling.
3. Documenta el preset en esta página.
