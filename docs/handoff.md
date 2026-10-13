# Handoff — estado de Shatranj / NetChessZX

Estado al 2026-10-13. Documento de arranque para cualquier sesión (Claude
Code, Codex, humano) que retome el proyecto: qué hay, qué está validado y
dónde, y qué falta. Mantenerlo al día en cada cierre de bloque; las
secciones «Pendiente» son la única lista de trabajo abierto. Sustituye los
resúmenes de sesión anteriores (estadísticas, recuperación) que ya se han
plegado en los documentos citados.

## 1. Estado del repositorio

- Rama `main`, release **1.3 «Sealed Move»** en preparación (2026-10-13).
  `VERSION` = `1.3`; `CHANGELOG.md` tiene la sección `[1.3]` para el usuario.
  Las notas de desarrollo del snapshot 1.2.1 siguen en el historial
  (`git show e324e4b:CHANGELOG.md`) y la evidencia física está en
  `docs/hardware-validation.md`.
- Publicación: este repositorio (`IgnacioMonge/netchesszx`, privado, Actions
  desactivadas; renombrado a minúsculas el 2026-10-13 porque GitHub Pages
  distingue mayúsculas en la ruta) se exporta en un único commit al público
  `IgnacioMonge/shatranj` (se renombra de `Shatranj` al publicar la 1.3), sin `.mex/`, `AGENTS.md` ni `work/` (desde la 1.3). Allí se
  lanzan los tres workflows de clientes Qt y se publican la release (`v1.0`
  a `v1.2` publicadas) y GitHub Pages con la estructura de SpecTalkZX:
  arranque directo en la raíz (con la portada como `index.html`) e instalador
  en `install/`, montado por `make spectranext-pages`
  (`docs/spectranext-direct.md`). Spectranext 1.3 exige firmware 1.0.
- Bloques recientes: arranque directo de Spectranext (`f3e0fca` a `e324e4b`;
  HTTP LAN PASS el 2026-10-13, HTTPS pendiente), réplica de la última
  respuesta a un reintento cruzado en Qt y Spectrum (`3701a2e`, `736302d`),
  sesiones de shrink y de margen de overlays del 2026-10-13
  (`docs/z80-optimization-ledger.md`) y plazos Y/N comunes con cancelación
  con el enlace vivo (§14).
- Gate de release: `make release-check` (`full-check` con
  `MAX_ALLOCS_PER_NODE=200000`). `make release` es solo la regla del
  directorio `release/`.
- Toolchain: z88dk `v24860-762578732` (`zcc` lo confirma), Python 3.10 o
  posterior con Pillow (`PYTHON=`), Qt 6 (`make client-test`).
- `build/` y `release/` están ignorados por git.

## 2. Qué hay en `main` (cronología reciente)

| Fecha | Commit | Contenido |
|---|---|---|
| 09-22 | `9f440dfa`, `7f80176b` | Diario STJ1 en disco, fixes de carga Classic, About ZX0 |
| 09-26 | `7d0e431d`, `bb3dd7f8` | Persistencia del diario y restauración; menos presión de memoria residente |
| 09-27 | `2f8414b5` | Registro del experimento de recuperación y auditorías A1–A4 |
| 09-29 | `85231782` | **Recuperación de partida interrumpida** en Classic, Next y SpectraNext (checkpoints `SRC2` + diario) |
| 09-30 | `61e0fbf9`, `5a8f57ae` | Reconexión DIRECT en SpectraNext (socket único), re-listen paced |
| 09-30/10-01 | `a8ca04b2` … `cdcf91c0` | Nombres de jugador (`MNAME`, 10 caracteres) en cabeceras MOVES, Setup con fila NAME, auditoría y hardening |
| 10-01 | `7b2b5297`, `2e152741` | Repintado de TIME al aceptar RTC en Setup; vector de aceptación |
| 10-01 | `8841c0cb`, `8bcbea66` | Arranque de partida simplificado; cabeceras DIRECT preservadas durante comandos |
| 10-02 | `af59b31f` | **Estadísticas de fin de partida** (Spectrum página a tablero completo, Qt diálogo), sin revancha automática, HINTS ON/OFF |
| 10-02 | `86b597b8` | **Reinicio post-partida solo para el host**; rechazar termina la sesión (NACK → BYE) |
| 10-09 | `b37816f` … `6d2ea35` | Shrink del residente (−552 B Classic): captura viva del checkpoint, tablas de tokens, parsers compartidos |
| 10-10/11 | `a890fc1` … `dac0076` | Menos cargas de overlay por jugada (C9, E2, E2b), caché de ranura `.RCV` (C10), reintento del cargador y corte si BOARD no carga (C3); **revisión pendiente, §13** |
| 10-11 | `89a1e70`, `1cbe957` | Qt: descarga de Stockfish con tag fijado si la API de GitHub rechaza; interruptor de Stockfish atenuado cuando está apagado |
| 10-11 | `bf12aec` | Qt con Stockfish: respuesta del motor descartada tras un prompt local vuelve a pedirse; reinicio único del motor caído; tope de tres NACK por posición; anuncio por chat con reintento; test `pc-engine-flow` con motor UCI de pega (incluye TAKEBACK del rival aceptado mientras piensa) |
| 10-12 | `ca964a8` … `22786a6` | Plazo común de 60 s para TAKEBACK, DRAW, RESET y LOAD; cancelación en segundo plano sin cerrar la sesión (§14) |
| 10-13 | `01c0996` … `071891e` | Shrink del residente y margen de overlays (GUI_LOG, SETUP, ABOUT); presupuesto SDCC 50000 por defecto |
| 10-13 | `3701a2e`, `736302d` | Réplica de la última respuesta a un reintento cruzado (Qt y Spectrum) |
| 10-13 | `f3e0fca` … `e324e4b` | **Arranque directo de Spectranext por HTTP(S)** (`docs/spectranext-direct.md`) |
| 10-13 | `552effa` y siguientes | Release 1.3: manual en los paquetes macOS/Linux, `make release-check`, versión, documentación |

Comportamiento vigente tras OVER (detalle y tabla de estados en
`docs/game-stats.md`): el host ve `S:STATS  RESTART? Y/N`, el invitado
`S:STATS  WAITING OPPONENT`; la petición del host llega como
`S:STATS ACCEPT RESTART? Y/N`; N/BREAK en cualquiera termina la sesión; una
petición caducada mantiene OVER. El reducer común sigue aceptando un RESET
consentido de cualquier rol (peers 1.2.1); la restricción al host es política
de los clientes.

## 3. Mapa de documentos

- Normativos: `docs/wire-contract.md`, `docs/session-core-contract.md`,
  `docs/source-layout.md`, `docs/architecture-decisions.md`,
  `docs/maintenance.md` (escalera de validación; `AGENTS.md` la replica).
- Diseño y evidencia por funcionalidad: `docs/game-stats.md`,
  `docs/game-journal-design.md`, `docs/game-recovery-experiment.md` +
  `docs/game-recovery-handoff.md`, `docs/spectrum-connection-config.md`,
  `docs/mqtt-session-policy.md`, `docs/next-graphics-contract.md`.
- Seguimiento: este fichero; `docs/post-refactor-backlog.md` (#11, #27–#29
  abiertos); `docs/hardware-validation.md` (evidencia física).
- Agentes: `AGENTS.md` (gate operativo, matriz de validación, comandos),
  `.mex/ROUTER.md` → `.mex/context/*.md` y `.mex/patterns/*.md`.
- Emulador: `docs/zesarux-zxespemu.md` (escrito para macOS; en Windows ver §6).
- Histórico no normativo: `docs/archive/` y `work/external-audits/` (este
  último solo en el repo privado; la exportación al público lo excluye).

## 4. Matriz de validación

Leyenda: **SW** = host tests/vectores ASM/guards (`make test`, `full-check`);
**EMU** = ZEsarUX + ZXESPEmu o FuseX; **HW** = hardware real confirmado por
Ignacio.

| Funcionalidad | SW | EMU | HW | Falta en HW |
|---|---|---|---|---|
| Estadísticas fin de partida (Spectrum/Qt) | ✅ | ✅ Classic host y guest (10-02) | desplegado, no probado | Reinicio entre dos máquinas, estadísticas tras FILE LOAD, página en Next/SpectraNext |
| Reinicio solo host, rechazo = BYE | ✅ | ✅ (10-02) | — | Dos máquinas; peer 1.2.1 mixto (RESET automático) |
| Nombres de jugador `MNAME` | ✅ | ✅ | — | Cabeceras con dos nombres de 10 caracteres en los tres Spectrum |
| Recuperación de partida | ✅ | ✅ | ✅ Classic+Next tras corte (09-28); SpectraNext tras reset (09-29) | Replay del historial en Next/SpectraNext; corte durante append/checkpoint; latencia SD y flash; Classic↔Next por DIRECT y MQTT |
| Diario STJ1 / FILE SAVE-LOAD-ERASE | ✅ | ✅ | parcial (09-28/29) | ERASE y lista de saves con las últimas builds |
| Reconexión DIRECT SpectraNext | ✅ | ✅ FuseX 8 ciclos | ✅ 09-30 (PKG `f915f7eb…`) | Binario simplificado posterior (sin estado redundante de driver) |
| UART divMMC/Next (`daa227f`) | ✅ | ✅ | ✅ 09-07 básico | Ráfagas/CTS, recuperación con errores inyectados |
| SpectraNext general | ✅ | ✅ | ✅ 09-10 | Inyección de fallo TX, high-water de stack |
| Backlog #1 TAKEBACK, #7 ventana 2,5 s, #9 reintentos PC tras BYE, #18 desconexión sucia ~21 s | ✅ | — | — | Revalidar end-to-end en hardware |
| Next: llamada exhaustiva de todos los overlays | — | — | parcial 09-02 | Recorrido completo de IDs |
| Qt con Stockfish: flujo del motor (`bf12aec`: prompts locales, caída, NACK, TAKEBACK del rival) | ✅ `pc-engine-flow` con motor de pega | — | — | Partida con el Stockfish real; ZX real pidiendo TAKEBACK con el motor pensando |
| TAKEBACK del rival contestado tarde en el Spectrum (backlog #31, 2026-10-11) | ✅ paridad `takeback-remote-slow-answer` | ✅ ZXESPEmu Classic DIRECT, Y y N a los 8 s | — | Classic, Next y SpectraNext reales contra el Qt con Stockfish; medir el SP gap de SpectraNext (+8 B residentes compartidos) |
| Arranque directo Spectranext (DAT retenido, OVL precargado) | ✅ vectores Z80 con ROM simulada (10 escenarios) | — | ✅ HTTP LAN (10-13) | HTTPS desde GitHub Pages; servidor apagado tras arrancar; ciclo de corriente; instalado sigue arrancando |
| Réplica de la respuesta a un reintento cruzado (`3701a2e`, `736302d`) | ✅ `pc-engine-flow`, transiciones Spectrum | — | — | RESET tras tablas Qt↔Spectrum sin petición doble |

## 5. Pendiente (lista única)

### 5.1 Validación física (orden sugerido)
0. Humo de la release 1.3 sobre los artefactos finales: ver
   `docs/hardware-validation.md` § «1.3 release». Cubre lo que el ledger da
   por pendiente tras las sesiones de shrink del 2026-10-13 (MQTT connect,
   scroll del chat, About y estadísticas, Next y SpectraNext).
1. Recompilar `main` (`make tap`, `make nex`, `make spectranext-port-build
   SPXN_DIR=C:/dev/SpectraNext/driver`) y desplegar (§6.3).
2. Partida completa Classic↔Next por DIRECT con nombres: OVER por tablas,
   estadísticas en ambos, reinicio del host aceptado; repetir con N (BYE en
   ambos) y con BREAK.
3. Lo mismo con un peer Qt como host y como guest; MQTT mixto.
4. Recuperación: corte de alimentación durante una partida con historial
   largo en Next y SpectraNext; comprobar lista de jugadas completa tras
   `Resume last game? Y`.
5. FILE SAVE / LOAD / ERASE en los tres Spectrum con las builds actuales;
   estadísticas tras LOAD (muestra parcial).
6. TAKEBACK pedido desde el Qt (con Stockfish o no) y contestado en el ZX
   pasados 5-10 s, con Y y con N, en Classic, Next y SpectraNext (backlog
   #31; fix del 2026-10-11 validado solo en ZXESPEmu Classic). Antes de
   desplegar SpectraNext, `make spectranext-size-report` en el equipo
   `C:\dev`: el fix añade 8 B al residente compartido y su SP gap estaba
   cerca del suelo de 512.
7. Backlog #1/#7/#9/#18 (TAKEBACK rechazado/expirado, reintentos PC tras
   BYE, caída sucia ≈21 s).
8. UART: ráfagas/CTS; SpectraNext: inyección TX y high-water de stack.
   Ráfagas probadas el 2026-10-03 con `tools/direct_burst_probe.py` (ver
   §9): con 1200 bytes de golpe el Classic pierde líneas, igual con el lector
   antiguo que con el nuevo. Causa medida: no es CTS ni el anillo UART
   (recepción pura sin pérdida hasta 8 líneas / 389 bytes); es la cola
   DIRECT de tres tramas, que descarta la cuarta línea cuando el Spectrum
   está contestando. Backlog #30. Quedan sin probar los errores inyectados.
9. Optimización del 2026-10-09/10 (`docs/z80-optimization-ledger.md`,
   secciones «Aggressive shrink session» y «Per-ply overlay loads»):
   - Classic y Next: hecho el 2026-10-11 (PASS, ver `docs/hardware-validation.md`): SAVE,
     LOAD y reanudación tras corte de corriente; en Classic una lectura de
     overlay de ~2 KB cuesta unos 30 ms (E2 + C9 quitaron dos: 60-70 ms).
   - E3 (#30): aplazado el 2026-10-11; la opción (b) bloquea el envío y
     solo queda la recepción pasiva (ver backlog #30).
   - E2b: `ACK <ply>` (y MOVE) se clasifican en residente solo en Classic
     (+53 B; Next −21 B), así que CONTROL ya no se carga al jugar; humo en
     Classic real PASS (`718ee3b`).
   - C10: el checkpoint de Classic y Next recuerda la ranura `.RCV` que
     escribió y verificó (3 B residentes) y no abre las dos: −2 aperturas y
     −4 lecturas por jugada. Verificado en ZXESPEmu (ver ledger), sin ronda
     en hardware por decisión del usuario.
   - C3: el cargador de Classic reintenta una vez una lectura fallida; si
     BOARD sigue sin cargar al deshacer o restaurar, la sesión termina como
     una pérdida de enlace con «OVL load failed», sin aplicar ni escribir
     checkpoint (+92 B Classic; en Next, +88 B hasta limitarlo a Classic,
     §13.3). Sin ronda en hardware: el
     emulador no puede fallar una lectura de SD; lo cubren vectores Z80 y
     tests host con fallo inyectado.

Registrar cada resultado como pass/fail/pending con commit y SHA-256 en
`docs/hardware-validation.md` (regla de `docs/maintenance.md`).

### 5.2 Software
- **Reparación local de §14.4 cerrada en software:** full-check y Qt en verde;
  falta la validación física de cancelación perdida y aceptación cruzada.
  La revisión Z80 de §13 es otro bloque.
- **SpectraNext con margen otra vez** (2026-10-04, `make
  spectranext-size-report`): RECOVERY 3472 de 4096, SAVELOAD 3718, SP gap
  533 (suelo 512). El 10-03 quedaba a un byte (4095) porque el adaptador
  `C:\dev\SpectraNext\driver\adapters\xfs_compat.asm` (1114 bytes, una copia
  por overlay) creció entre `ae8573c` y `611d43a`. Ahora cada copia omite lo
  que el overlay no llama, con los interruptores `SPXN_XFS_NO_*` del driver.
  Requiere el commit `30bf0f4` del driver (`C:\dev\SpectraNext`, `main`, sin
  push); con un driver sin interruptores el producto enlaza como antes.
- Resuelto el 2026-10-03: los tests de paridad DIRECT de SpectraNext vuelven a
  compilar y pasan (a sus reglas del Makefile les faltaba
  `-DNETCHESSZX_RECOVERY`). Siguen fuera de `make test`; corren con
  `spectranext-conformance-test`.
- Backlog: #11 (círculo de conexión Next, cosmético); #27–#29 opcionales,
  solo con medición enlazada y sin crecimiento (ver criterios en el backlog).
- Recrear si hacen falta los scripts auxiliares no versionados perdidos con el
  worktree `feature/usage-stats`: `build/game_stats_emulator_smoke.py`,
  `deploy_tnfs.py`, `guest_client.py`, `repl.py`, `hashes.py`,
  `statsN_gate.cmd`. Si se recrean, versionarlos bajo `tools/` o
  `tests/tools/`, no en `build/`.
- Qt con Stockfish, Preferencias y manual (2026-10-06, rama
  `claude/play-against-machine-adec48`; diseño en
  `docs/superpowers/specs/2026-10-06-qt-preferences-engine-help-design.md`):
  validado en Windows (17/17 en `make client-test`, partida por loopback,
  paquete con `tls/qschannelbackend.dll` descargando por HTTPS). Pendiente:
  en un Mac, que `macdeployqt` incluya el plugin TLS y el menú
  *Preferences…* (`Q_OS_MACOS`); y ver en un Spectrum real el nombre
  `STOCKFISH` en la cabecera, el aviso por chat y la barra inferior
  `VS <plataforma>: STOCKFISH`, p. ej. `VS MAC: STOCKFISH` (Classic, Next y SpectraNext; overlay `MENU_LOGIC`,
  residente sin cambios).
  - Revisión del flujo con motor (2026-10-11, commit `bf12aec`): una
    respuesta de Stockfish que llegaba con un prompt local abierto (Draw,
    Resign, Reset) se descartaba y no se volvía a pedir; ahora toda respuesta
    limpia la petición pendiente y el cierre del prompt vuelve a preguntar
    (`confirmLocalRequest`). Si el motor muere (crash o salida) se reinicia
    una vez con la misma identidad y nivel; si vuelve a morir antes de mover,
    la etiqueta de turno dice `STOCKFISH STOPPED - <lado>` y el operador
    rinde o desconecta. Si el rival rechaza (NACK) la jugada del motor tres
    veces en la misma posición, el cliente deja de preguntar y muestra lo
    mismo; una nueva posición (takeback, reset, restore) reinicia la cuenta.
    El anuncio por chat (`<MOTOR> PLAYS <lado> - LEVEL n`) reintenta cada
    segundo mientras dure la partida en vez de rendirse a los 3 s. Verificado
    también que un TAKEBACK del rival aceptado por el operador mientras
    Stockfish piensa descarta la respuesta antigua, revierte la jugada, acepta
    la nueva del rival y la contesta. Todo cubierto por `pc-engine-flow`
    (`tests/pc/test_engine_flow.cpp`, seis casos) con un motor UCI de pega
    (`tests/pc/fake_uci_engine.cpp`) y un peer DIRECT guionizado; 18/18 en
    `make client-test`. Sin probar con el Stockfish real tras el cambio.
- Release 1.3: ver §1 y §5.1 punto 0. Tras publicarla, abrir de nuevo
  `## [Unreleased]` en `CHANGELOG.md` para el siguiente ciclo.

### 5.3 Documentación
- `docs/zesarux-zxespemu.md` sigue escrito para la ruta macOS; añadir la
  variante Windows (`C:\dev\ZXESPEmu\run.cmd`, UART TCP local).
- Mantener este fichero y `.mex/context/*` (`last_updated`) en cada cierre.

## 6. Entorno de trabajo

Ignacio trabaja desde varios ordenadores, cada uno con su propio checkout de
`origin/main`. Las rutas `C:\dev` de este apartado son las de **un** equipo
Windows concreto; no existen en los demás. Antes de leer este fichero o de
trabajar, haz `git pull` (los otros equipos también hacen push), y antes de
usar una herramienta de §6.1 comprueba que existe en el equipo actual.

| Equipo | Checkout | Notas |
|---|---|---|
| Windows `C:\dev` | `C:\dev\NetChessZX` | Entorno completo de §6.1: SpectraNext, ZXESPEmu, skills Z80, despliegue |
| Windows `DESKTOP-C8JP3E4` | `E:\Desarrollo\GitHub\ignaciomonge\NetChessZX` | Sin `C:\dev`. Herramientas en `E:\Desarrollo\Herramientas`, fuera del PATH: abrir `Abrir-desarrollo.cmd` y `Entrar-Proyecto NetChessZX` (z88dk fijado en `z88dk-24860`, make/gcc de w64devkit, Qt 6.11.0 msvc2022_64, VS2022 Build Tools; Python 3.14, no el 3.11.9 fijado). `make full-check` en verde el 2026-10-11. El driver SpectraNext local no tiene `30bf0f4` |
| Mac | — | Midió los tamaños de §13 (su z88dk enlaza el residente Classic ~7 B por encima del de Windows); único equipo con ZXESPEmu + ZEsarUX montados para `tools/emu_ply_trace.py` (§13.2) |

### 6.1 Rutas (equipo Windows `C:\dev`)
- Repo: `C:\dev\NetChessZX` (único checkout de ese equipo; no quedan worktrees).
- SpectraNext backend/driver: `C:\dev\SpectraNext` (`driver/`, `tools/dev.py`,
  `tools/deploy.ps1`, `tnfsd/`). Los targets SpectraNext exigen
  `SPXN_DIR=C:/dev/SpectraNext/driver`; `spectranext-port-build` genera el
  instalador SPXI v2 con `tools/dev.py installer`.
- Emulador: `C:\dev\ZXESPEmu` (`run.cmd`, `launcherctl.cmd`,
  `stage_netchess.py`; en Windows la UART es TCP local
  `tcp:127.0.0.1:<puerto>`). Launchers del repo: `zxespemu-launchers.json`
  (Classic, Next, SpectraNext/FuseX).
- Skills Z80 del usuario: `C:\dev\z80-skills` (canónico), enlazadas en
  `~/.claude/skills` (`route-z80`, `optimize-z80`, `send-bridgezx`,
  `port-spectranext`, …).
- Hardware de pruebas: BridgeZX → ZX Classic `192.168.0.199`, Spectrum Next
  `192.168.0.122`; SpectraNext por TNFS local (paquete `SHATRANJ.PKG`).
- Cliente Qt empaquetado: `C:\dev\NetChessZX\release\shatranj-client\`.

### 6.2 Comandos (ver matriz completa en `AGENTS.md` / `docs/maintenance.md`)
- `make test` — host + vectores ASM, sin zcc. `make module-guards` — guards.
- `make tap` / `make nex` — Classic TAP+OVL+DAT / Next NEX en `release/`.
- `make full-check` — cierre canónico Spectrum/host (una sola invocación;
  no encadenar `abi-check`/`size-check`/`tap`/`nex` alrededor).
  En este `C:\dev` con w64devkit, usar
  `make full-check PYTHON=C:/PROGRA~1/PYTHON~1/python.exe`: alias corto
  comprobado del mismo Python 3.11. La ruta con espacios se corrompe al
  propagar MAKEFLAGS a los sub-makes de las pruebas (2026-10-12).
- `make client-test` — única vía para compilar/probar el cliente Qt.
- `make tap-spectranext SPXN_DIR=C:/dev/SpectraNext/driver` y
  `make spectranext-port-build SPXN_DIR=C:/dev/SpectraNext/driver` — solo si
  la petición nombra SpectraNext.

### 6.3 Despliegue
- Classic/Next: skill `send-bridgezx` o cliente BridgeZX; Classic necesita
  TAP+OVL+DAT juntos (DAT que no case → borde rojo `DAT?`); Next solo el NEX.
- SpectraNext: copiar el `.PKG` al TNFS con copia de seguridad y SHA-256
  anotado; instalar con SPXI v2 (el paquete ≈50 KB supera SPXI v1).
- Verificar en emulador antes de enviar a hardware.

### 6.4 Emulador (notas operativas de las últimas sesiones)
- Classic/Next: `C:\dev\ZXESPEmu\run.cmd --program netchess --target
  classic|next --netchess C:\dev\NetChessZX`; peer DIRECT de escritorio en
  `127.0.0.1`.
- Scripting ZEsarUX por remote: BREAK = `send-keys-event 135` + `128`;
  flechas 142–145. Al terminar, cerrar `zesarux` y `zxesp` y borrar el
  fichero/puerto `uart` residual.
- FuseX/ZEsarUX no tienen contrato de automatización interactiva: nada de
  Computer Use ni clicks; validar por logs y memoria/pantalla del emulador.

### 6.5 Reglas de trabajo exigidas por Ignacio
- Git **solo desde Windows** (PowerShell/Desktop Commander), nunca desde una
  VM Linux sobre la carpeta montada (muestra todo el árbol como modificado).
  Commit solo cuando lo pida, en `main`, y push; mensajes imperativos cortos.
- Sin ventanas de consola visibles ni sondeos repetidos; builds en una cadena
  `cmd` con log y fichero de estado, esperando con `sleep`; sin tareas
  programadas para seguir builds.
- Mejor solución, no la mínima; sin diagnósticos sin solución; no presentar
  hipótesis como hechos; evidencia de hardware siempre «pendiente» hasta que
  él la confirme.
- Seguir `AGENTS.md`: gate operativo, un solo gate de cierre por bloque, nada
  de builds de plataformas no pedidas.

## 7. Riesgos y gotchas conocidos
- BridgeZX Next puede dejar el ESP a 230400 baudios tras NMI/reset
  (`.mex/patterns/debug-direct-mqtt.md`).
- SpectraNext: `SAVELOAD` 3718 de 4096 bytes (estaba a 3 B del límite); cada
  jugada escribe en flash. SP gap 518, a 6 B del suelo de 512 (medido el
  2026-10-04 tras el ACK adelantado, §12): cualquier byte residente nuevo
  en código compartido debe medirse con `make spectranext-size-report
  SPXN_DIR=C:/dev/SpectraNext/driver`. Next: fallo de recuperación por
  buffers esxDOS en `0x3C78` ya corregido (buffers en memoria del overlay).
- Config Spectrum con cola de nombre de 11 B: un build 1.2 la lee como
  `CONFIG INVALID`; ficheros 1.2 cargan sin nombre.
- Spectrum LOAD acepta solo STJ1; saves provisionales de 60 B rechazados.
- Peers 1.2.1 antiguos con revancha automática: documentado en
  `docs/wire-contract.md`; validación mixta pendiente.
- CHANGELOG cita `build/uart_*_hardware_test.json` que ya no existen (eran
  no versionados); se dejó constancia, no hay copia.

## 8. Cambios de documentación del 2026-10-03

Solo documentación; sin cambios de código, binarios ni baselines. Cierre según
la matriz: «Documentation or cosmetic text → review the diff → Stop».

- `docs/handoff.md` (nuevo, este fichero); enlazado desde `AGENTS.md`,
  `docs/README.md`, `docs/README.es.md`.
- `docs/game-recovery-handoff.md` reescrito: ya no cita la rama
  `claude/game-recovery` sin commit; recoge el merge `85231782`, la evidencia
  HW del 09-28/09-29 y la lista pendiente real.
- `docs/game-recovery-experiment.md`: línea de estado actualizada.
- `README.md` / `README.es.md`: «either player can request … restart» corregido
  (el reinicio es del host).
- `docs/session-core-contract.md` y `.mex/context/protocol-session.md`:
  precisan que el reducer acepta RESET de cualquier rol pero los clientes lo
  ofrecen solo al host; `.mex/context/spectrum-runtime.md`: evidencia de la
  página RECOVERY en hardware; ambos `last_updated: 2026-10-03`.
- Notas de hardware (hoy en `docs/hardware-validation.md`): añadidas las entradas del 09-28/09-29
  (recuperación) y 10-02 (estadísticas desplegadas, validación EMU); corregida
  la referencia a los JSON perdidos; nota de recuperación en «Added» ajustada.
- `docs/README.md` / `docs/README.es.md`: sección «Estado actual y
  seguimiento»; versión 1.2.x.
- `docs/post-refactor-backlog.md`: nota de revisión 2026-10-03.
- `docs/game-stats.md`: el registro de verificación cita los commits de
  integración.

Commit «Refresh project state docs and add handoff», autorizado por Ignacio el
2026-10-03. Siguiente paso al retomar: §5.1 punto 1.

## 9. Cambios de código del 2026-10-03

- Menú: al enfocar un grupo (NTWRK, OFFER, THEME) la etiqueta rueda directa a
  su primera acción; al salir vuelve a la etiqueta genérica.
- About (Classic y SpectraNext): banda de 19 filas en las filas 3-21, sin la
  línea «A CHESS GAME FOR ZX», con fila en blanco y aviso «PRESS C FOR
  CHANGELOG, ANY TO CLOSE». Al cerrar se repinta la etiqueta de turno.
- Página de novedades: C en el About. Texto puro en el overlay de estado
  (`asm/overlay/menu_logic/changelog.asm`, entrada 2); se edita ahí en cada
  versión. Next no la tiene y conserva su About Layer 2.
- Classic: pila en el tope de RAM (`REGISTER_SP=0`) y los dos buffers de
  recepción DIRECT en `0xfd01`, sobre el vector IM2. SP_GAP 599 → 671 y 45
  bytes libres bajo el vector.
- Baselines actualizados a propósito: tamaño y ABI (constante
  `SPECTRUM_OVL_STATUS_CHLOG` y dos símbolos residentes exportados).
- Validación: `make full-check` en verde. Ignacio probó en ZX Classic real
  (2026-10-03, por BridgeZX en `192.168.0.198`) el menú, el About y varias
  iteraciones de la página de novedades; el último retoque (aviso inferior en
  fuente ikkle amarilla) no se ha probado en hardware. Partida DIRECT completa
  con la pila y los buffers nuevos: pendiente de confirmación explícita.
  SpectraNext: código compartido, sin construir ni probar.

Segundo commit del mismo día:

- Página de novedades: escritor de texto propio (descodifica cada letra una
  vez) y título dibujado en negro antes de estirarlo, para que no aparezca
  primero en tamaño normal. Cuatro líneas del texto se acortaron para caber.
- Cabecera MOVES: solo se repinta la etiqueta cuyo texto cambia. Dos bytes de
  clave residentes (`spectrum_moves_header_shown`), a cero cuando
  `draw_moves_header` borra la fila.
- Qt: el botón Game stats es un icono cuadrado de barras.
- Medido: residente Classic 35741 bytes, SP_GAP 669, MENU_LOGIC 2047 de 2048.
  `make full-check` y `make client-test` (14 de 14) en verde.
- Sin probar en hardware: el escritor de texto nuevo, el título estirado y el
  repintado selectivo de la cabecera. BridgeZX no respondía en el último
  intento de envío.

Tercer bloque del mismo día (`e7a24c04`, `b9d25f48` y el commit de la sonda):

- Integrado el parche de la revisión de acceso a overlays
  (`C:\dev\NetChessZX-audits\overlay-management\2026-10-03\`): una sola
  lectura por overlay en Classic y dos recargas menos por jugada. Residente
  Classic 35708 bytes (−33).
- Recorte de 6 bytes en el código de recuperación; ver §5.2.
- `make full-check` en verde; `make tap-spectranext` enlaza.
- Prueba de ráfagas en el ZX Classic real (`192.168.0.198`, DIRECT HOST,
  puerto 5000), sonda como invitado con partida guionizada. Build de
  referencia `0cf37b1e` frente a la de lectura única: mismas pérdidas bajo
  ráfaga de 1200 bytes, enlace siempre vivo, y la jugada reenviada se acepta
  en unos 1,78 s frente a 2,05 s. Datos en `docs/evidence/` y en
  `docs/hardware-validation.md`. El ahorro de tiempo por jugada en juego
  normal sigue sin medir.
- Ignacio vio en el ZX el nombre del rival en la cabecera y la build con el
  Changelog nuevo cargada; no ha dado veredicto visual sobre ellos.

Cuarto bloque del mismo día, tras las pruebas de Ignacio en el ZX y en Qt:

- Cabecera MOVES: el repintado completo ocurría al conectarse el rival, porque
  `game_message_loop` reconstruía los paneles laterales siempre. Ahora solo si
  no están visibles (`spectrum_gui_side_panels_visible`).
- Estadísticas ZX frente a Qt: el Spectrum reiniciaba el reloj de turno al
  final del proceso de la jugada y dejaba unos 3 s por jugada sin contar (25 s
  frente a 34 s en tres jugadas). El reinicio va ahora justo después de
  `spectrum_journal_after_move`; ver `docs/game-stats.md`.
- Setup: aceptar un SET distinto marca la configuración como modificada
  (`spectrum_setup_accept_set`) y pide repintar ACTION en el mismo paso
  (overlay SETUP), de modo que EDIT pasa a SAVE sin mover el cursor.
- Qt: Game stats es un conmutador de barras entre Load game y Log, encendido
  mientras la página está visible; funciona durante la partida y se actualiza
  tras cada jugada. Sin botón Back ni cierre por tecla. Nombres de la cabecera
  de jugadas alineados a la izquierda.
- Medido: residente Classic 35724 bytes (+16), SP_GAP 669, SETUP 2033 de 2048.
- El acceso directo `NetChessZX Qt.lnk` abre `release\shatranj-client`: tras
  tocar el cliente hay que cerrar con `make client`, no solo `client-test`.
- Enviado al ZX; pendiente de veredicto de Ignacio: cabecera al conectar,
  comparación de estadísticas y EDIT/SAVE al cambiar SET.

## 10. Cambios del 2026-10-04 (auditoría Z80 de los cambios del 10-02/03)

Sin defectos por encima de LOW. Aplicados los tres puntos menores:

- Fila de estado: si el overlay STATUS no carga al pintar, la fase queda
  marcada (`SPECTRUM_STATUS_STALE`) y la siguiente jugada repinta, en vez
  de saltarse el repintado toda la partida. Afecta a `status_show_phase` y
  al aviso de máquina del rival (MACH). Solo Classic puede fallar ahí.
- `tools/check_lowmem_layout.py` rechaza un build Classic cuyos slots DIRECT
  sobre el vector IM2 no midan `SPECTRUM_LINK_PAYLOAD_MAX` (48).
- Comentarios corregidos: estadísticas de 33 bytes y origen del título del
  changelog.
- Medido: residente Classic 35738 bytes (+14), SP_GAP 669; Next residente
  32938, SP gap 3245, banco de extensión con 848 libres. ABI Classic y Next
  sin cambios. Baseline de tamaño actualizado a propósito.
- Validación: host tests y guards en verde; `size-check` Classic, `abi-check`
  y `nex-size-report` en verde. Un primer intento del NEX falló porque otra
  sesión editó `src/common/ui_messages.h` durante el build (el atlas firma
  `src/common`); el reintento pasó. SpectraNext se recompiló después (punto
  siguiente).
- Sin probar en hardware.
- SpectraNext: el adaptador XFS del driver gana interruptores de tamaño
  (`SPXN_XFS_NO_*`, commit `30bf0f4` del driver). Ningún overlay usa
  `esx_freplace` (−375 B en ABOUT, CONFIG, CONTROL, FILEUI, RECOVERY y
  SAVELOAD) y RECOVERY quita además directorio, creación, update y commit
  (−623 B en total). Medido con `make spectranext-size-report`: RECOVERY
  3472, SAVELOAD 3718 de 4096. El driver por defecto sale byte a byte igual
  (`make adapter-check` en verde, incluida la superficie mínima).

## 11. Cambios del 2026-10-04 (cliente Qt y DRAW rechazado)

- Barra inferior del cliente Qt: el estado dice la fase; el centro solo el
  aviso pendiente, la acción tras OVER y `VS <máquina>`. Fuera la dirección,
  `Side`, `Ply`, `Last` y los avisos que repetían el panel lateral. Textos
  sin jerga (`ACK`, contadores MQTT), una forma `Waiting for X`, dos textos de
  desconexión y peticiones del rival como `Opponent asks to …`. Solo cambian
  macros que el Spectrum no usa (`src/common/ui_messages.h`).
- DRAW rechazado: un reenvío del DRAW que se cruzaba con el `NACK DRAW`
  abría una segunda pregunta. Ahora un DRAW rechazado sigue rechazado hasta
  que cambia la posición (MOVE, TAKEBACK, RESET, START o RESTORE) y se
  contesta `NACK DRAW` sin preguntar. Contrato, reductor canónico (MQTT y
  DIRECT) y FSM compacta (`CONTROL_REJECT_DRAW` en `last_control_accept`,
  sin RAM nueva); transcripción `mqtt-control-remote-draw` y escenario
  DIRECT `draw-reject-latch-guest`.
- Medido: residente Classic 35749 bytes (+11, baseline actualizado a
  propósito, z88dk `762578732c`); Next residente 32949 (+11). `make
  full-check` y `make client-test` en verde.
- Probado en ZXESPEmu el 2026-10-04 (ZX Classic host por MQTT, PC invitado)
  y en un ZX Classic real en ambos sentidos con `d9460659`: sin segunda
  pregunta tras rechazar. PASS, hashes en `docs/hardware-validation.md`.

## 12. Cambios del 2026-10-04 (ACK de jugada adelantado)

- Motivo: la espera del ACK de una jugada había crecido. El mensaje `MOVE` no
  cambia; el Spectrum receptor hacía antes del ACK todo el trabajo de la
  jugada: dos parpadeos de 0,48 s, lista de jugadas, diario y checkpoint
  con `fsync` y cargas de overlay. En el Classic real (sonda del 10-03) el
  ACK tardaba 1,72–1,83 s con un PING de ida y vuelta de unos 0,14 s.
- Ahora el receptor envía `ACK <ply>` justo tras aplicar la jugada al
  tablero y hace después la lista, el parpadeo del destino, el diario y el
  checkpoint (`SESSION_APP_APPLY_REMOTE_MOVE` + `SESSION_APP_FINISH_REMOTE_MOVE`
  en `session_ports.h`; orden en `transitions.inc`). Si el ACK no sale, se
  termina igual y luego se corta el enlace. El parpadeo del origen de la
  jugada del rival se mantiene antes del ACK (Ignacio quiere ver de dónde
  sale la pieza). La jugada propia ya no parpadea en origen tras el ACK.
- Los jueces de paridad observan la entrega de la jugada remota en el
  commit del tablero (`HOST_SESSION_OBSERVE_REMOTE_MOVE`), no en el
  repintado; los resultados esperados no cambian. Documentado en
  `docs/session-core-contract.md`. Test nuevo de orden en
  `tests/spectrum/test_session_transitions.c`.
- Recuperación: un corte entre el ACK y el checkpoint deja al receptor una
  jugada por detrás; la restauración usa el checkpoint del host, como ya
  pasaba si el corte afectaba al emisor tras el ACK.
- Medido: residente Classic 35753 bytes (+4, baseline actualizado a
  propósito; SP_GAP 669 sin cambio); Next 32953, SP_GAP 3230; SpectraNext
  36162, SP gap 518 (suelo 512). `make full-check` en verde;
  `make spectranext-size-report` enlaza.
- Enviado al ZX Classic (`192.168.0.198`) por BridgeZX: TAP
  `da600d0357887ab8f7273f39b3eba73466088071562e53d853145fb0d2eb7a3d`, OVL
  `aa241fccbfff7974090486ec03f1e5949ea456fbd9873173798da3d254f8beb0`, DAT
  `23dad24acc6ab1fdfbe6b0956413cbab57028406d2be1ddaadad293746d392e6`.
  Ignacio no quiere más pruebas de latencia en hardware para este cambio.
- Corregido el mismo día: un árbol nuevo (clon, worktree o tras `make
  clean`) no compilaba el TAP Classic. `tools/package_classic_boot.py` pasaba
  a `z80asm` la ruta absoluta del fuente; `z80asm` dejaba el objeto en
  `build/C/<ruta>/asm/spectrum/`, carpeta que nadie creaba (y con rutas
  largas superaba los 260 caracteres de Windows). Ahora pasa la ruta
  relativa y crea `build/asm/spectrum/`. Comprobado con `make tap` en un
  worktree nuevo: TAP idéntico al de `main`.

## 13. Revisión pendiente: optimización Z80 del 2026-10-09/11

Todo está en `main` y en `origin/main`. Detalle y medidas en
`docs/z80-optimization-ledger.md` (secciones «Aggressive shrink session» y
«Per-ply overlay loads»); resultados de hardware en `docs/hardware-validation.md`.
Tamaños medidos en macOS: el z88dk de ese Mac enlaza el
residente Classic 7 B por encima del de Windows, así que en `C:\dev` espera
unos 7 B menos en Classic.

### 13.1 Qué revisar, de más a menos riesgo

1. **C3, `dac0076`: reintento del cargador y corte si BOARD no carga.**
   - `asm/esxdos/overlay_loader.asm`: `ovl_load` llama a `ovl_load_try` y,
     si falla, cae en un segundo intento; el intento fallido ya cerró el
     fichero, así que el segundo reabre, posiciona y lee. Solo Classic.
   - `src/spectrum/board/board.c/.h`: `spectrum_board_snapshot_restore` y
     `spectrum_board_undo_restore` devuelven el resultado del overlay
     (0 = no cargó, nada cambió). El build de host tiene
     `spectrum_board_host_fail` para inyectar el fallo.
   - `src/spectrum/app/app.c`: `apply_takeback_snapshot` y
     `saveload_apply_snapshot` devuelven 0 antes de tocar ply, lista,
     diario o checkpoint. `recovery_checkpoint()` pasa a estar dentro de
     `saveload_apply_snapshot` (los tres puertos lo llamaban justo después).
     `disconnect_message` deja que el siguiente `handle_opponent_disconnected`
     diga «OVL load failed» en vez de «OPPONENT DISCONNECTED».
   - `src/spectrum/session/transitions.inc/.h` y `session_ports.h`: cinco
     salidas. Aceptar un takeback (ruta de teclas) sale por `goto failed`;
     el ACK de nuestro takeback, un RESTORE recibido y un RESTORE aceptado
     (RA) salen por la etiqueta `board_lost` (aviso + desconexión +
     `dispatch_exit`). Un LOAD sin rival solo muestra LOAD FAIL. Se borró el
     puerto `SESSION_APP_RESTORE_LOCAL_RESULT`: la rama RA reutiliza
     `RESTORE_DECODE_VALID` y `APPLY_LOADED`.
   - Tras el corte, DIRECT vuelve a escuchar o reconecta y el ESP cierra el
     enlace viejo: el rival ve el corte. «Resume last game» solo se ofrece
     si el Spectrum que falló era el host.
   - Coste: residente Classic +92 B (35302 → 35394), Next +88 B; BSS y
     SP_GAP sin cambio.
2. **C10, `177b193`: caché de la ranura `.RCV`.** 3 B en
   `asm/spectrum/journal.asm` (`_spectrum_rcv_cache`, datos, no BSS);
   `asm/overlay/recovery/entry_recovery.asm` salta el escaneo de las dos
   ranuras si la caché es válida y la invalida si la escritura falla. Toca
   la ruta que protege frente a cortes de corriente. Alta en
   `tools/gen_overlay_defs.py` y en las dos baselines ABI (solo esa
   entrada). SpectraNext no cambia.
3. **E2b, `718ee3b`, y E2, `aa0453c`:** `src/spectrum/session/event.c`
   clasifica `MOVE` y `ACK <ply>` sin cargar CONTROL, solo en Classic.
4. **C9, `a890fc1`:** aviso de turno del rival mientras GUI_LOG sigue
   cargado (`app.c`, `finish_applied_move`).
5. **Shrink `b37816f` … `6d2ea35`:** ya pasó la ronda de recuperación en
   Classic y Next reales (§5.1, punto 8).

### 13.2 Cómo verificar

- Un solo gate: `make full-check` (no lo precedas ni sigas de `abi-check`
  ni `size-check`). Esperado en macOS: Classic residente 35394 B, BSS 415,
  SP_GAP 669; Next 32515 B, SP_GAP 3668.
- Focalizados, sin zcc: `python tests/tools/test_classic_overlay_dispatch.py`
  (vector `ovl_load` con fallos inyectados: carga limpia, un fallo y
  reintento, dos fallos), `python tests/tools/test_recovery.py` (caché C10),
  `make test` (paridad de sesión con fallo de BOARD inyectado en los dos
  takebacks y en un RESTORE aceptado).
- `tools/emu_ply_trace.py` necesita ZXESPEmu y ZEsarUX (solo está
  montado en el Mac). Cuenta cargas y E/S por jugada y guarda las ranuras
  `.RCV` tras un corte simulado; no mide latencia de SD.

### 13.3 Huecos conocidos

- **SpectraNext no se ha compilado** en esta optimización (ni el shrink ni
  C3): `app.c`, `board.c` y las macros de sesión son compartidas. Cuando
  toque, `make spectranext-port-build SPXN_DIR=C:/dev/SpectraNext/driver` y
  `make spectranext-size-report` (SP gap con suelo 512; último medido, 518
  el 2026-10-04).
- C3: la salida de un RESTORE recibido no tiene test propio (comparte la
  etiqueta `board_lost` con el RA, que sí lo tiene). Nada de C3 ni de C10
  se ha probado en hardware: el emulador no puede hacer fallar una lectura
  de SD, y para C10 Ignacio prefirió el emulador.
- C3 costaba +88 B en Next, donde un overlay casi no puede fallar. Ya está
  limitado a Classic (`SPECTRUM_BOARD_RESTORE_CAN_FAIL` en `board.h`): Next
  y SpectraNext compilan la aplicación sin comprobar de antes de C3. `make full-check` en verde en Windows (`DESKTOP-C8JP3E4`): Classic
  35387 B (sin cambio), Next 32412 B y SP_GAP 3771 (en macOS con C3: 32515 y
  3668). Falta `make spectranext-size-report`, con el driver en `30bf0f4`.
- C10: una SD cambiada o editada sin reiniciar no se detecta.

### 13.4 Opciones abiertas (no hechas)

- C3: donde el Spectrum aplica antes de responder (aceptar un takeback,
  RESTORE recibido) se podría contestar `NACK`/`RN` en vez de cortar.
- #30: la pérdida de líneas DIRECT solo se arregla con recepción pasiva
  del ESP (`AT+CIPRECVMODE=1`); aplazado.

## 14. Plazos Y/N comunes (2026-10-12)

**Estado publicado en `588a2a6`: incompleto; reparación local en §14.4.**
Se comunicó como terminado tras pasar los gates,
pero el usuario rechazó la desconexión que todavía ocurre cuando nadie
confirma la cancelación. Pasar los tests actuales no demuestra ese requisito.
Tras su objeción solo se hicieron lecturas hasta la petición de handoff;
no se aplicó otra corrección. El usuario pidió este handoff para continuar en otra
sesión; después autorizó commit y push a `main`. No se modifica más código
en este cierre.

### 14.1 Requisito y estado real

- Requisito autorizado: 60 segundos de tiempo real para TAKEBACK, DRAW,
  RESET (también tras OVER) y aprobación de LOAD, independientes de los
  polls y de los reintentos. TAKEBACK sin respuesta debe rechazarse,
  conservando tablero, conexión y posibilidad de seguir jugando. La falta
  de confirmación de su cancelación tampoco debe causar ese cierre.
- Implementado: timer común de decisión en el reducer; reloj de frames
  50/60 Hz en Spectrum; los reintentos no reinician el plazo. LOAD pasa de
  15 a 60 segundos para la aprobación; RN solo cancela antes de RY.
- Se reutilizan RN y CANCEL DRAW/RESET. TAKEBACK añade `CANCEL TAKEBACK <ply>`:
  el rival invalida la pregunta y responde NACK; si ya había aplicado el undo,
  reenvía su ACK para que el solicitante haga el mismo undo. Con NACK recibido
  se conserva la partida. **Sin respuesta a la cancelación, cinco reintentos
  adicionales siguen acabando en desconexión**, incluso entre peers
  actualizados si se pierde la respuesta. No es solo un problema de versiones.
- Rutas concretas pendientes: `direct_handle_control_timeout()` en
  `src/common/session/direct_session.c:2654`,
  `mqtt_handle_local_control_timeout()` en
  `src/common/session/mqtt_session.c:3688` y `SESSION_IDLE_STEP` en
  `src/spectrum/session/transitions.inc:402`. DRAW/RESET cancelados también
  alcanzan el agotamiento genérico; revisar el criterio común en esas rutas.
- Mientras espera confirmación, TAKEBACK sigue siendo `pending_control` /
  `takeback_pending_ply`. Evitar el cierre dejando esa espera indefinida no
  basta: también hay que permitir que la partida continúe.
- Riesgo observado en el código: el receptor aplica el undo antes de emitir
  ACK. Ignorar un ACK que cruza la cancelación o liberar sin más el estado
  pendiente puede dejar tableros distintos. Los tests actuales preservan la
  aplicación de ese mismo undo en ambos lados. **No se ha elegido una solución
  para retirar la solicitud sin confirmación y resolver esa carrera.**
- El prompt remoto actualmente no caduca por sí solo: lo cierra CANCEL o el
  cierre de sesión. No presentar la política como resuelta en ambos extremos.
- El Spectrum usa el contador de frames existente, a 50/60 Hz, con resta
  modular de 16 bits. La actividad de red no detiene los plazos. El scheduler
  pasa de 6 a 7 bytes de stack, sin nueva BSS ni arena de memoria baja.
  Los reintentos de transporte mantienen su cadencia anterior y no definen
  el plazo de aprobación.

### 14.2 Cambios y evidencia reutilizable

- El usuario autorizó publicar este estado en `main`; sin despliegue.
  Consultar `git log` y el diff frente a `ca964a85`, no rehacer el bloque.
  Afecta a protocolo común, reducers DIRECT/MQTT, scheduler y
  dispatch Spectrum, CONTROL, aviso Qt, fixtures/tests, contratos/manuales,
  guard de límites y export ABI. El helper común
  `netchess_proto_takeback_ply()` usa `NETCHESSZX_FASTCALL` en declaración y
  definición; el macro es vacío en desktop.
- Cobertura añadida: plazo compartido, retirada y respuestas cruzadas de
  TAKEBACK, validación de ply y retained, frames sin tiempo transcurrido,
  trabajo largo/desbordamiento y cierre del prompt Qt con el motor pensando.
- Gates del árbol actual en verde: `make full-check
  PYTHON=C:/PROGRA~1/PYTHON~1/python.exe`
  (guards, host/ASM, ABI de ambos targets, límites, BOARD real-entry e IY
  enlazados) y `make client-test` (18/18). Paridad MQTT 78/78 en Classic/Next
  a 50/60 Hz y corpus DIRECT en verde. **Falta la regresión de cancelación
  sin confirmar con el enlace aún vivo y continuación de la partida.**
- Cierre Git: se reutilizó el full-check aprobado, con código, toolchain y
  configuración sin cambios. Se corrigieron las advertencias de los manuales,
  contratos y memoria, y se volvió a ejecutar `make client-test`: 18/18.
- Medidas finales: Classic residente 35739 B, BSS 415, SP_GAP 669, CONTROL
  1935; Next residente 32770 B, BSS 513, SP_GAP 3413, CONTROL 2143. Classic
  queda 14 B bajo su baseline y Next supera el suelo SP_GAP 3072. No se han
  ampliado límites ni regenerado las baselines completas; solo se añade el
  export del parser común, a 0xA59C/0xA550.
- El reloj de frames se limita a la aprobación: convertir también los
  reintentos de transporte excedió el presupuesto Classic. Las iteraciones
  y decisiones están en `docs/z80-optimization-ledger.md`. Conservar los
  límites existentes; quedan solo 14 B de margen residente Classic.
- Evitar repetir problemas ya resueltos: en w64devkit usar el alias corto de
  Python indicado en §6.2; los espacios se corrompen en MAKEFLAGS de sub-make.
  El fixture DIRECT a 60 Hz necesitó 260 ticks para cruzar su deadline.
- Sin prueba física de este bloque ni medida high-water de stack.
  SpectraNext no se ha compilado; no hay procesos de build pendientes.

### 14.3 Plan histórico de `588a2a6` (completado en software en §14.4)

1. Añadir una regresión enfocada: petición TAKEBACK sin respuesta durante
   60 s, CANCEL/NACK perdido o no contestado, PING/ACK PING todavía funcional.
   Cruzar el agotamiento de cancelación y exigir sesión activa, misma posición
   y siguiente jugada aplicada/confirmada en ambos lados. Ejecutarla contra
   el árbol actual para demostrar el fallo; no empezar con otro full-check.
2. Resolver esa ruta con el cambio mínimo completo en común y Spectrum.
   Cubrir CANCEL repetido, ACK aceptado justo antes del vencimiento y perdido,
   respuesta tardía y nueva solicitud/jugada. No sustituir el cierre por un
   bloqueo permanente ni eliminar la detección de pérdida real del enlace.
   Comprobar también DRAW/RESET y LOAD bajo el plazo común.
3. Actualizar los tests, fixtures de paridad, contratos, manuales, CHANGELOG
   y `.mex` que hoy documentan la desconexión como política aceptada. El
   usuario no la aceptó. No dar la tarea por terminada solo por pasar los
   tests antiguos.
4. Tras los checks enfocados, cerrar con una sola invocación de
   `make full-check PYTHON=C:/PROGRA~1/PYTHON~1/python.exe` si se toca
   residente/ABI; añadir `make client-test` según la matriz de AGENTS.md.
   No encadenar gates de firmware ni compilar SpectraNext sin petición.
5. Validación física aún pendiente: los cuatro permisos en Classic/Next
   50/60 Hz, cancelación sin confirmar, ACK cruzado y high-water de stack.
   La revisión Z80 de §13 sigue pendiente y es otro bloque; no mezclarla.

En aquella sesión se autorizaron commit y push a `main` al cerrar;
despliegue no autorizado. Prioridad del usuario: menos diagnóstico y más
acción; la sesión anterior consumió casi dos horas y dejó este requisito
sin cumplir. Retomar desde el fallo concreto, no desde una auditoría general.


### 14.4 Reparación de cancelación con el enlace vivo (2026-10-12)

Commit y push autorizados por el usuario al cerrar este bloque; sin despliegue
a Spectrum. Reproduce el fallo de `588a2a6` en un test DIRECT:
PING seguía funcionando pero el sexto reintento cerraba la sesión y no podía
confirmarse la siguiente jugada. Reparación en común y Spectrum:

- La UI se libera tras el primer TX de CANCEL; la correlación sigue en segundo
  plano, sin cierre por agotamiento de esos reintentos. Los relojes y detección
  de pérdida real del enlace siguen activos.
- El receptor también cierra la pregunta y rechaza a los 60 s sin necesitar
  CANCEL. Duplicados no reinician ese plazo; un Y posterior es inerte.
- Una jugada del rival puede avanzar y su commit retira la cancelación. DRAW y
  RESET caducados permiten también una jugada propia aún sin aplicar; un ACK
  de aceptación cruzado puede descartarla y completar el control original.
  Si se confirma la jugada, el control viejo queda invalidado; si se rechaza,
  vuelve la retirada correlacionada.
- Si RESET/DRAW ya se aceptó, CANCEL obtiene su ACK, igual que TAKEBACK, en vez
  de un NACK que afirmaría erróneamente que la aceptación se retiró.
- No cambian verbos, ficheros, límites ni almacenamiento persistente Spectrum.
  El timer y las rutas de CANCEL existentes cubren el deadline remoto.
  Un peer antiguo que ignore CANCEL y no tenga deadline remoto necesita que
  su operador cierre la pregunta; no se le puede imponer el nuevo comportamiento.

Evidencia enfocada: regresión original roja antes del cambio y verde después;
test de dos peers con todos los CANCEL/NACK descartados, siguiente MOVE/ACK
confirmado y ambos plies iguales; corpus compartido DIRECT y MQTT con retiro
sin confirmar, respuesta tardía y nueva solicitud; reloj remoto/local 50/60 Hz
con wrap y sin rearmar por duplicados. Cierre en macOS:

- PASS: `make full-check PYTHON=/tmp/netchesszx-cancel-python/bin/python`:
  guards, host/ASM, ABI Classic/Next, tamaños y BOARD/IY enlazados. MQTT
  81/81 en Classic/Next a 50/60 Hz; corpus DIRECT en verde; 3218 vectores
  nuevos de las rutinas nativas (reloj, cancelación, avisos y fallo TX).
- PASS: `make client-test` (18/18) y `make client`; Qt firmado/verificado e
  instalado en `/Applications/Shatranj.app`. Común/Qt no cambió después de
  esos checks; los ajustes posteriores fueron solo Spectrum.
- Classic: residente 35751 B frente al máximo 35753 (2 B libres), BSS 415,
  SP_GAP 669, CONTROL 1935. Next: residente 32751, BSS 513, SP_GAP 3432
  frente al mínimo 3072, CONTROL 2143. No se modificaron límites ni baselines.
- El primer candidato C falló el suelo Next (SP_GAP 2965). Se compactaron
  helpers privados y llamadas con fastcall/callee, conservando un oracle C
  para host y vectores Z80 independientes. El primer intento de gate con
  Python 3.9 falló en el test de imágenes existente: se usó un venv temporal
  Python 3.11 con Pillow; no se cambiaron dependencias globales ni Makefile
  para eludir ese test.
- Binarios locales para la siguiente prueba física: `release/SHATRANJ.tap`
  (SHA-256 `5fa5fa33919f2fcee90280eaab9c3605d728a09a5151b7a7324ebb07f215c8a3`)
  y `release/Next/SHATRANJ.nex`
  (`ead7c3f518950a0739f4499cec7b2aab6468cfa9ef921950e6aca51aa6f3cf91`).
  No se enviaron a dispositivos. Se eliminó el worktree temporal de reproducción.

Medidas y opciones: última sección de `docs/z80-optimization-ledger.md`.

Pendiente físico: Classic/Next 50/60 Hz, cancelación perdida, aceptación cruzada
y high-water de stack. SpectraNext no se compila en este bloque. §13 sigue
pendiente por separado. No usar las instrucciones históricas de §14.3 para
volver a aplicar una reparación sobre esta; continuar desde el estado actual.
