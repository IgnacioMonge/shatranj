# Documentación técnica de Shatranj

[English](README.md) · [Guía de usuario](../README.es.md) · [Guía del cliente Qt](../client/README.es.md)

Este directorio contiene la documentación de ingeniería mantenida para
Shatranj 1.3. Los dos contratos normativos son deliberadamente independientes
del transporte y del cliente:

- [`wire-contract.md`](wire-contract.md): payloads, framing TCP Direct por
  líneas, topics MQTT, intercambio restore y reglas de compatibilidad.
- [`session-core-contract.md`](session-core-contract.md): estado de sesión,
  reducers, reintentos, acknowledgements y semántica entre clientes.

## Arquitectura y responsabilidades

- [`source-layout.md`](source-layout.md): límites y propiedad de los módulos.
- [`architecture-decisions.md`](architecture-decisions.md): decisiones
  transversales aceptadas y su motivo.
- [`maintenance.md`](maintenance.md): reglas soportadas de build, validación y
  mantenimiento de releases.

## Estado actual y seguimiento

- [`handoff.md`](handoff.md): dónde está el proyecto, qué está validado y
  dónde, y la lista única de trabajo pendiente. Léelo primero al retomar.
- [`hardware-validation.md`](hardware-validation.md): resultados en hardware
  real con builds y hashes de los artefactos.
- [`post-refactor-backlog.md`](post-refactor-backlog.md): peticiones
  numeradas, decisiones y experimentos opcionales.
- [`game-stats.md`](game-stats.md): estadísticas de fin de partida y reinicio
  solo para el anfitrión, con el registro de verificación.
- [`game-journal-design.md`](game-journal-design.md): diario de jugadas en
  disco y saves STJ1.
- [`game-recovery-experiment.md`](game-recovery-experiment.md) y
  [`game-recovery-handoff.md`](game-recovery-handoff.md): recuperación de
  partida interrumpida: diseño, costes, evidencia en hardware y pendientes.
- [`spectrum-connection-config.md`](spectrum-connection-config.md): pantalla
  Setup, RTC de respaldo e invariantes del estado SAVE.
- [`spectranext-direct.md`](spectranext-direct.md): arranque directo de
  Spectranext por HTTP(S): paquete, vida de los recursos y reglas de
  publicación.
- [`zesarux-zxespemu.md`](zesarux-zxespemu.md): harness local ZEsarUX y
  ESP-UART virtual.

La implementación es común para Qt Windows/macOS/Linux, ZX Spectrum Classic,
Spectrum Next y SpectraNext. El C común posee las reglas de ajedrez, protocolo, sesión y
formato de guardado; el código de escritorio las adapta a Qt y los clientes
Spectrum usan su runtime compacto por target. El parseo y la construcción del
protocolo deben permanecer en common; los dos contratos anteriores son la
fuente de verdad.

## Puntos de entrada de compilación y validación

Ejecuta desde la raíz del repositorio:

```sh
make test          # pruebas de host
make client-test   # build y pruebas Qt
make tap           # TAP/OVL/DAT de Classic
make nex           # NEX autocontenido de Next
make full-check    # guards cross-target de release
```

El cliente Linux está soportado y probado; se compila contra el Qt del sistema.
El workflow **Build Linux AppImage** empaqueta x86_64, conserva los artefactos
de ejecuciones manuales; adjuntarlos a una release publicada es un paso
manual aparte. Las
variables de configuración Spectrum son `PORT`, `MQTT_HOST`, `MQTT_PORT`
y `MQTT_CODE`; úsalas solo para un build Spectrum configurado. El flujo
público Qt usa CMake mediante los targets del repositorio; no documentamos
fallback qmake ni CMake raw.
