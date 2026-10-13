# Handout — recuperación de partidas Shatranj

Estado al 2026-10-03. La recuperación está integrada en `main` desde
`85231782` (2026-09-29, «Recover interrupted games on Classic, Next and
SpectraNext»); la rama experimental `claude/game-recovery` ya no existe.
Diseño, formato, costes y evidencia: [game-recovery-experiment.md](game-recovery-experiment.md).
Los contratos son [wire-contract.md](wire-contract.md) («Game recovery») y
[session-core-contract.md](session-core-contract.md). Las auditorías A1–A4 de
`work/external-audits/game-recovery/` (archivo privado, no publicado)
evaluaron el plan anterior; son evidencia histórica, no instrucciones. El estado global del proyecto y la
lista única de validación pendiente viven en [handoff.md](handoff.md).

## Comportamiento integrado

- Oferta automática del host tras peer-ready, `Resume last game? Y/N`,
  aceptación por el camino LOAD/RESTORE existente, historia completa del host
  cuando el enlace TMP verifica y `RESTORED` en otro caso. Sin verbos nuevos.
- Checkpoint `SRC2` de 73 B en dos `.RCV` alternos, enlazado a
  `SHATJRNL.TMP` por cuenta de registros y CRC. Se escribe tras cada jugada,
  TAKEBACK, carga/restauración, GAME START (ply 0, retira la oferta) y final
  de partida (OVER, no ofrecible).
- Una salida elegida por cualquiera de los dos (desconexión del menú, que el
  otro recibe como BYE) cierra la partida para la recuperación: no hay oferta.
  Pérdida de enlace, reset o corte la conservan. Partidas terminadas o
  sustituidas por START nunca se ofrecen.
- Classic, Next y SpectraNext repintan el historial en la lista de jugadas
  tras FILE LOAD o recuperación (antes Next/SpectraNext mostraban solo
  `RESTORED`).
- `/recover` y el prototipo `SRC1` se retiraron, igual que el hito 1 del plan
  del 2026-09-26 (oráculo `recovery_tmp_format.py`, wrappers `esx_recovery_*`,
  modelos de barrera y el parser `GC1`–`NE1`).

## Decisiones fijadas (2026-09-28)

- Dos RCV enlazados al TMP y consentimiento RESTORE, en lugar de un único TMP
  con eventos de 73 B y barrera propia (~7 KiB residentes).
- Un fallo de invalidación avisa `Recovery failed` y se sigue jugando; tras un
  corte podría ofrecerse la partida anterior y responder N la descarta.
- Solo el host ofrece; el invitado recibe posición, flags y relojes GAME/TURN
  y queda con historia parcial; rechazar no toca diario ni checkpoints; STJ v1
  y la profundidad de TAKEBACK no cambian.

## Evidencia en hardware

- 2026-09-28, Classic (esxDOS) y Next (NEX): la partida se restaura tras
  cortar la alimentación. El fallo inicial de Next (`Recovery failed` en cada
  START, `LOAD FAILED` tras FILE SAVE; buffers esxDOS en `0x3C78` con MMU1 en
  ROM durante `RST 8`) quedó corregido con buffers en la memoria del overlay y
  la prueba se repitió con éxito.
- 2026-09-29, SpectraNext: instalación con SPXI v2, arranque tras reinicio y
  recuperación correctas; la lista mostraba solo `RESTORED` (tres jugadas).
  El replay del historial se añadió después y no se ha repetido en hardware.

## Pendiente

- Repetir en Next y SpectraNext la recuperación con el replay del historial
  (lista de jugadas completa, no solo `RESTORED`).
- Corte de alimentación durante append y checkpoint, latencia SD, UART/CTS
  durante el stage, Classic↔Next por DIRECT y MQTT.
- SpectraNext: `O_WRONLY|O_TRUNC` sobre el TMP en flash, latencia de la
  escritura en flash por jugada, apagado real y el resto de un corte entre
  commit y `RENAME` (código de la librería SpectraNext, no de este producto).
- RA perdido tras aplicar en el invitado sigue la semántica existente de
  RESTORE (reintentos y desconexión); al reconectar el host vuelve a ofrecer.
- Qt no ofrece recuperación. SpectraNext sí (checkpoints con reemplazo
  atómico y diario en flash); SAVELOAD quedó a 18 B del límite de 4 KiB en
  `85231782` y a 13 B tras las estadísticas (`docs/game-stats.md`).

Para continuar: leer `AGENTS.md`, `.mex/ROUTER.md`, `docs/handoff.md`, este
handout y el diseño; seguir `docs/maintenance.md`.
