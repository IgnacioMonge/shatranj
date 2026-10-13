# Diario de partida — dossier de producto

Stage: spec
Mode ceiling: auto
Demand: Standard
Status: M0 COMPLETE
Canonical dossier: `docs/game-journal-design.md`
Spec accepted: user, 2026-09-16 (auditorías Gemini/Claude/DeepSeek/Codex/GLM; se rechaza STJ v2)
Goal accepted: user, conversación 2026-08-14
Auto-advance milestones: no

## Objetivo

Cada partida genera incrementalmente en el almacenamiento local un historial
completo de movimientos comprometidos sin conservarlo en RAM. Al guardar, se
crea un `.STJ` que contiene ese historial y una instantánea final suficiente
para cargar inmediatamente la posición. La partida puede continuar y los
guardados posteriores incorporan los nuevos movimientos.

El valor para el jugador es conservar la partida completa sin aumentar la RAM
con la duración de la partida ni alterar el juego de red.

### Corte mínimo convincente

En Classic y Next, jugar una partida con movimientos locales, remotos y un
TAKEBACK; guardarla; verificar externamente que el `.STJ` contiene exactamente
esa secuencia; cargarlo y continuar jugando desde el tablero y los relojes
restaurados. Classic reconstruye la lista desde un diario local completo;
una carga parcial o un RESTORE remoto muestra la frontera `RESTORED` (D-14).

### Fuera de alcance del diario v1

La [recuperación de partida](game-recovery-experiment.md) (D-16) añade dos
checkpoints alternos enlazados a `SHATJRNL.TMP` y lo ofrece tras reconectar.
Conserva el formato `.STJ` y el registro de 16 bits; las exclusiones
siguientes describen el alcance original de v1.

- Recuperar automáticamente el temporal después de corte eléctrico, reset o
  cierre del programa.
- Cabeceras A/B, generaciones y checkpoints periódicos.
- Reproducir o navegar el historial completo en Spectrum.
- Generar PGN en Spectrum.
- Transferir el diario al rival durante RESTORE o cambiar el protocolo de red.
- Registrar timestamps por movimiento, chat, tráfico, retransmisiones,
  movimientos rechazados, DRAW, RESIGN o RESET.
- Cargar el formato provisional de 60 bytes. El nuevo `.STJ` v1 es el único
  formato de fichero soportado.

## Perfil de plataforma

### Targets y entrega

- Classic 48K: `SHATRANJ.tap`, `SHATRANJ.OVL` y `SHATRANJ.DAT`, con esxDOS.
- Spectrum Next: `SHATRANJ.nex`, usando el mismo contrato de fichero.
- Cliente Qt: mismo `.STJ` v1; implementación de almacenamiento propia.
- C99 y Z80 ASM para Spectrum; C++17/Qt para escritorio; sin dependencias
  nuevas.

### Restricciones

- Cada overlay continúa limitado a 2048 bytes y no se anidan overlays.
- El estado persistente residente del diario no debe crecer con la partida.
- Objetivo de BSS residente nuevo: como máximo 8 bytes para estado, contador,
  CRC y registro pendiente; debe demostrarse mediante artefactos enlazados.
- Un fallo de almacenamiento nunca cambia tablero, turno, resultado, ACK/NACK
  ni política de sesión.
- El I/O del diario no puede provocar retransmisiones, pérdida UART, caída de
  CTS, expiración de watchdog ni desconexión.
- DIRECT, MQTT y RESTORE siguen transmitiendo únicamente el estado de partida
  actual. El diario es persistencia local.

## Definiciones de producto

### Movimiento comprometido

Un MOVE entra en el diario exactamente una vez cuando ya forma parte del
tablero local:

- movimiento local: al aplicar el pendiente tras su ACK, o mediante la ruta
  equivalente que confirma implícitamente el movimiento;
- movimiento remoto: después de validarlo y aplicarlo, aunque falle el envío de
  su ACK posterior;
- duplicado o retransmisión: no añade registro;
- movimiento rechazado o todavía pendiente: no añade registro.

Un TAKEBACK aceptado añade `UNDO_ONE` exactamente una vez después de restaurar
el tablero. Una retransmisión del TAKEBACK no añade otro registro.

### Historial completo y parcial

`HISTORY_COMPLETE` significa que el cuerpo parte del comienzo real de la
partida y contiene todos sus MOVE y UNDO comprometidos.

El historial pasa a parcial si falla una escritura o si el estado procede de un
RESTORE remoto que no trae diario. Desde ese momento no se vuelve a declarar
completo durante esa partida. El tablero continúa siendo guardable y cargable.

### Ciclo de vida

1. Un GAME START confirmado declara una partida nueva con historial vacío, pero
   no toca todavía el fichero temporal.
2. El primer registro comprometido crea o trunca el temporal y lo escribe.
3. Los registros siguientes se añaden al mismo temporal.
4. Mate, ahogado, abandono, desconexión o vuelta a configuración conservan el
   diario de la partida terminada.
5. Un GAME START posterior cambia lógicamente a un historial nuevo; el temporal
   anterior solo se trunca cuando se compromete el primer registro nuevo.
6. Guardar una partida sin registros crea un `.STJ` válido con cuerpo vacío.
7. Cargar un `.STJ` copia su cuerpo válido al temporal antes de permitir la
   continuación. Si esa copia falla, se carga la posición y la continuación se
   marca como historial parcial.
8. Un peer que recibe únicamente el RESTORE de red carga la posición, pero su
   diario local comienza como parcial porque no recibió el historial anterior.

## Formato `.STJ` v1

No existe ruta de compatibilidad. Los enteros multibyte son little-endian.

```text
offset 0   cabecera fija, 76 bytes
offset 76  diario, 2 bytes por registro
EOF        exactamente 76 + record_count * 2
```

### Cabecera

| Rango | Tamaño | Contenido |
|---|---:|---|
| 0..3 | 4 | magic ASCII `STJ1` |
| 4 | 1 | versión, `1` |
| 5 | 1 | flags; bit 0 = `HISTORY_COMPLETE`, resto cero |
| 6..7 | 2 | tamaño de cabecera, `76` |
| 8..9 | 2 | número de registros |
| 10..11 | 2 | CRC-16 del diario |
| 12..13 | 2 | reservado, cero |
| 14..15 | 2 | CRC-16 de cabecera |
| 16..75 | 60 | estado restaurable Base64URL actual |

Ambos CRC usan CRC-16/CCITT-FALSE: polinomio `0x1021`, valor inicial `0xffff`,
sin reflexión y `xorout 0`. Para calcular el CRC de los bytes 0..75 de la
cabecera, los bytes 14..15 se consideran cero. El CRC de un diario vacío es
`0xffff`.

El estado Base64URL se conserva porque ya es el payload de RESTORE y ya tiene
codec compacto en Spectrum; no se conserva por compatibilidad con ficheros
anteriores.

La cabecera restaura tablero, turno, enroques, en-passant, color del host,
estado de partida, orientación y relojes. Classic reconstruye las filas
visibles a partir del cuerpo local completo, aplicando MOVE/UNDO y mostrando
`--:--` porque el diario no contiene tiempos por jugada. Si el historial es
parcial, no existe cuerpo o su lectura falla, muestra `RESTORED` en la columna
correspondiente al snapshot. Next, SpectraNext y el receptor de RESTORE remoto
mantienen la frontera `RESTORED`; el diario no se transmite por red.

### Registro de 16 bits

MOVE, con bit 15 a cero:

```text
bits 0..5    casilla origen 0..63 (fila*8 + columna; a8 = 0, h1 = 63)
bits 6..11   casilla destino 0..63
bits 12..14  promoción: 0 ninguna, 1 dama, 2 torre, 3 alfil, 4 caballo
bit 15       0
```

CONTROL, con bit 15 a uno:

```text
bits 0..3    opcode
bits 4..14   argumento
bit 15       1
```

En v1 solo existe `UNDO_ONE`, opcode 0 y argumento 0. Un valor de promoción,
opcode, argumento o bit reservado desconocido invalida el historial desde ese
registro, pero no invalida una instantánea de cabecera correcta.

## Temporal y guardado

El temporal contiene solo los registros del diario, sin cabecera. En Spectrum
usa un nombre 8.3 fuera del filtro `.STJ`, por ejemplo
`/SYS/CONFIG/SHATJRNL.TMP`.

Guardar es completamente secuencial:

1. Capturar el estado restaurable y los relojes mientras la aplicación no
   procesa otro movimiento.
2. Crear el destino `.STJ` y escribir sus 76 bytes de cabecera.
3. Copiar exactamente `record_count * 2` bytes del temporal al destino.
4. Cerrar y validar magic, campos, tamaño exacto y ambos CRC.
5. Mostrar éxito solo tras la validación; después continuar el mismo diario.

No se hace prepend, no se reescribe el temporal y no se necesita `F_SEEK` para
construir el `.STJ`. El append del temporal es un manejador persistente
(D-07): un `F_OPEN` por partida y `F_WRITE` de 2 bytes por registro. El handle
vive en residente; el BSS del overlay no sobrevive al page-out. No se
promueven wrappers de `F_SEEK` a CONFIG (D-06/D-08: JOURNAL usa
`esx_fseek_set`).

Si una escritura del diario falla o devuelve menos de dos bytes, se conserva el
último `record_count` y CRC válidos, se marca el historial parcial, se desactiva
el append durante esa partida y se notifica una sola vez. Los guardados
posteriores copian únicamente el prefijo válido y siguen restaurando el estado.

## Carga

1. Validar magic, versión, flags, campos reservados, tamaño exacto
   (`76 + record_count * 2`), CRC de cabecera y CRC del diario, todo antes de
   tocar estado alguno.
2. Aplicar la instantánea; Classic reconstruye las filas de un diario completo
   legible o muestra `RESTORED` según el contrato anterior.
3. Copiar el cuerpo validado al temporal (vía `SHATJRNL.NEW`) para habilitar
   su continuación.

Cualquier fallo de validación rechaza la carga sin modificar la instantánea
del llamante, el handle vivo, `pend`, el temporal ni `SHATJRNL.NEW`. No existe
detección automática ni ruta de compatibilidad con el snapshot de 60 bytes.
El host puede enviar el payload de 60 bytes del estado al peer mediante el
RESTORE existente; el diario no viaja por red y el receptor conserva la
presentación `RESTORED`.

## Criterios de aceptación

| ID | Tipo | Criterio | Verificación |
|---|---|---|---|
| AC-P-01 | Producto | El `.STJ` contiene en orden exactamente un registro por MOVE comprometido y por TAKEBACK aceptado. | Partida controlada y decodificador de referencia. |
| AC-P-02 | Producto | Guardar y cargar restaura tablero, turno, derechos, en-passant, color, estado, orientación y relojes; Classic reconstruye filas solo con cuerpo local completo legible; parcial/remoto conserva `RESTORED` (D-14). | Comparación antes/después en Classic y Next, local y con peer. |
| AC-P-03 | Producto | Guardar a media partida no termina ni reinicia el diario; el siguiente guardado contiene también los movimientos posteriores. | Dos guardados de una misma partida. |
| AC-P-04 | Producto | Rechazos, pendientes y retransmisiones no duplican el historial; TAKEBACK lo revierte una sola vez. | Casos DIRECT y MQTT existentes ampliados con observación del journal. |
| AC-P-05 | Producto | Cualquier fallo de almacenamiento deja intactos juego y sesión, avisa una vez y permite guardar/cargar el snapshot como historial parcial. | Inyección de fallos open/write/short-write/close. |
| AC-P-06 | Producto | Cargar y continuar conserva el historial si el `.STJ` local es válido; un RESTORE remoto produce explícitamente historial parcial local. | Flujos local y de dos peers. |
| AC-T-01 | Técnico | Codec v1 rechaza tamaños, flags, registros, CRC y campos reservados inválidos sin leer fuera de límites. | Vectores host positivos, negativos y truncados. |
| AC-T-02 | Técnico | La captura se engancha después del commit real en todas las rutas local/remota/TAKEBACK. | Trazado de rutas y pruebas de sesión enfocadas. |
| AC-T-03 | Técnico | `SAVELOAD` permanece ≤2048 bytes; BSS nuevo ≤8 bytes; límites de residente, stack y overlays pasan. | Mapas enlazados, `make size-check` y reportes Next/NEX. |
| AC-T-04 | Técnico | El append no introduce retry, NACK, desconexión, overflow UART ni fallo CTS en 100 plies con red activa. | Sonda hardware Classic y Next, DIRECT y MQTT. |
| AC-T-05 | Técnico | El guardado valida tamaño y CRC del destino antes de informar éxito. | Fallos de copia y corrupción inyectada. |
| AC-T-06 | Técnico | Wire/session grammar, payload RESTORE de 60 bytes y semántica de ACK/NACK no cambian. | Transcripts y `make full-check`. |
| AC-T-07 | Técnico | Qt lee y escribe el mismo formato y corpus de registros. | `make client-test` con vectores compartidos. |

Producto y codec siguen `NOT RUN` hasta M1. AC-T-04 queda cubierto para la
estrategia elegida en Classic DIRECT (sonda M0, no 100 plies). MQTT, Next y
el camino reopen no se miden más en M0.

## Ledger de opciones

| Opción | Evidencia/coste esperado | Riesgo | Decisión |
|---|---|---|---|
| Temporal solo con registros; construir destino como cabecera + cuerpo | Elimina prepend, checkpoints, 1024 bytes reservados y seek de cabecera | Requiere copiar al guardar | **Hacer** |
| Cabecera reservada dentro del temporal | Solo aporta valor a recuperación tras corte, fuera de alcance | Más I/O, formato y estados | **Rechazar** |
| Cabeceras A/B y checkpoints | No ayudan al objetivo aceptado | Complejidad, CRC/generación y replay | **Rechazar en v1** |
| Registro de 2 bytes + CRC-16 final | 400 bytes para 200 plies; estado residente constante | No localiza corrupción intermedia | **Hacer** |
| CRC-8 dentro de cada registro | Añade 50 % al cuerpo y código por una recuperación no requerida | Coste recurrente | **Rechazar en v1** |
| `TIME_MARK` | Útil para análisis temporal futuro, no para historial de jugadas | Amplía semántica y pruebas | **Diferir** |
| Manejador persistente del temporal | T-00: 0 frames / 100×2 B. T-01: 26 registros DIRECT (25 MOVE + UNDO_ONE), UART viva | Handle en residente; SAVELOAD no debe clobberear `journal_fd` | **Hacer (D-07)** |
| Abrir/seek/escribir/cerrar por registro | T-00: 2 frames / 100×2 B; más RST 8 por ply | No se usa en v1; no promover `esx_saveload_seek.asm` | **Rechazar en v1** |
| Instantánea completa por jugada | Rama anterior llegó a 1989/2048 en SAVELOAD y +193 B BSS sin terminar | No conserva historial y presiona RAM/overlay | **Rechazar** |

Mismo comportamiento visible; menor coste total mediante temporal sin cabecera,
registros de 2 bytes, CRC único y reutilización del payload RESTORE existente.

## Evidencia

| ID | Afirmación | Clase | Riesgo | Ancla/frescura |
|---|---|---|---|---|
| E-01 | SAVELOAD 1042/2048; JOURNAL 1944/2048 (104 B libres); CONFIG 2033/2048 (15 B libres); BSS 421; SP gap 512 (suelo 512); stack-guard gap 168; resident 35763 | VERIFIED | HIGH | `docs/size_report.baseline.json`, `make size-check`, 2026-09-20 |
| E-02 | El cargador Spectrum actual lee y escribe 60 bytes | VERIFIED | MEDIUM | `src/spectrum/overlay/saveload_ovl.c`, árbol 2026-08-14 |
| E-03 | Qt exige actualmente un fichero de 60 bytes | VERIFIED | LOW | `src/pc/client/save_game_store.cpp`, árbol 2026-08-14; se reemplazará, sin compatibilidad |
| E-04 | El commit local ocurre en `apply_pending_local_move`; el remoto antes de enviar ACK | VERIFIED | HIGH | `src/spectrum/app/app.c`, árbol 2026-08-14 |
| E-05 | La carga actual reinicia los logs y añade `RESTORED` en ambos peers | VERIFIED | MEDIUM | `src/spectrum/app/app.c`, árbol 2026-08-14 |
| E-06 | Classic persistente: T-00 0 frames (100×2 B, sin UART); T-01 26 registros en DIRECT vivo (e2e4…h8h2 + TAKEBACK `8000`) sin NACK/desconexión. Reopen T-00 2 frames, no elegido. MQTT/Next no medidos | VERIFIED | MEDIUM | Hardware Classic 2026-09-17/18; `SHATJRNL.TMP` 200 B = 26 T-01 + cola T-00 sin truncar |
| E-07 | Handle persistente sobrevive a GUI_LOG→SAVELOAD (escrituras secuenciales entre overlay page-in). fd en residente, no en BSS de overlay | VERIFIED | MEDIUM | T-01 spike `spike/journal-m0`, 2026-09-18 |
| E-08 | Gancho T-01 residente (~168 B CODE+DATA) deja SP gap 352; suelo producto 512. SAVELOAD sonda 1976/2048. No fusionar el spike | VERIFIED | HIGH | TAP T-01 2026-09-18: BSS_END `0xfdf8`, `main @ 9135828` SP 520 |
| E-09 | Next con gancho y codec (`JOURNAL_HOOK=1 STJ_SAVELOAD=1`): NEX de 17 páginas y sin overlay JOURNAL; residente 32683; SP gap 3494 (suelo 3072); SAVELOAD 2879/4096; ABI Next y Classic sin cambios | VERIFIED | HIGH | `build/nex/size_report.json`, `make abi-check`, 2026-09-20 |
| E-10 | SpectraNext S1 (`JOURNAL_HOOK=1`, `ATLAS_EXTRA=TIME`): atlas de 18 overlays, TIME id 17 y sin JOURNAL; SAVELOAD 3434/4096 con append ASM enlazado; lowmem OK (SP gap 877); append usa reopen-update + `esx_fseek_end` | VERIFIED | HIGH | `build/spectranext/`, autoridad `a6b5c88`, 2026-09-19 |
| E-11 | SpectraNext S2 (`JOURNAL_HOOK=1 STJ_SAVELOAD=1`): LOAD/SAVE usan el codec STJ1 compartido con streaming XFS de un handle; toda ruta y cada registro de append se copian fuera del overlay antes de paginar la ROM XFS; SAVE escribe y revalida el temporal antes de commit y rename; SAVELOAD 3887/4096 (209 B libres), TIME id 17, sin JOURNAL, SP gap 870 | VERIFIED | HIGH | `build/spectranext/overlay_sizes.json`, `build/spectranext/SHATRANJ.map`, `build/spectranext/size_report.json`, `make spectranext-size-report`, 2026-09-20 |

## Milestones

| ID | Resultado inspeccionable | Puerta de finalización | Rollback | Estado |
|---|---|---|---|---|
| M0 | Sonda esxDOS compara las dos estrategias de append con red activa | E-06/E-07 resueltas y estrategia elegida sin violar AC-T-04 | Descartar sonda/worktree | **DONE** |
| M1 | Classic y Next generan un temporal cuyo decoder reproduce MOVE/UNDO exactos | AC-P-01, AC-P-04, AC-T-01..04 | Revertir entradas y estado journal | **SOFTWARE** (HW Classic DIRECT persist) |
| M2 | Spectrum guarda, valida, carga y continúa `.STJ` v1 | AC-P-02, AC-P-03, AC-P-05, AC-P-06, AC-T-05/06 | Revertir codec/entradas SAVELOAD | **SOFTWARE + ABI** (Classic JOURNAL 1944; Next SAVELOAD 2879; SPXN SAVELOAD 3887; hardware pendiente) |
| M3 | Qt comparte formato, diario y corpus de pruebas | AC-T-07 y criterios de producto aplicables | Revertir persistencia Qt | PENDING |

## Tareas del hito M0

| ID | Tipo | Resultado | Dependencias | Superficie máxima | AC | Check | Estado |
|---|---|---|---|---|---|---|---|
| T-00 | SPIKE | Sonda descartable que escribe registros de 2 bytes mediante handle persistente y mediante open/seek/write/close | Especificación aceptada | Worktree descartable; wrapper esxDOS y telemetría mínima | AC-T-04 | Classic: persist 0 frames, reopen 2, n/got/sum 100 200 4950. Next no corrido | **DONE** |
| T-01 | VERIFY | Medir ambas estrategias con 100 plies en DIRECT y MQTT, Classic y Next | T-00 | Hardware y capturas externas | AC-T-04 | Classic DIRECT persistente: 26 registros + TAKEBACK, partida usable. 100 plies/MQTT/Next no exigidos para elegir | **DONE** |
| T-02 | SPIKE | Elegir estrategia y demostrar su coste enlazado antes de promover código | T-01 | Dossier y artefactos de la sonda | AC-T-03/04 | Persistente (D-07). Spike +168 B residente, SP 352, no promover. Seek no entra en producto | **DONE** |

## Decisiones y revisiones

- D-01: el objetivo aceptado es historial completo sin RAM más snapshot de
  carga; recuperación tras corte no forma parte de v1.
- D-02: no hay compatibilidad con el fichero provisional de 60 bytes.
- D-03: el temporal no lleva cabecera; el destino se construye secuencialmente.
- D-04: v1 registra jugadas y TAKEBACK, no tiempo ni otros eventos.
- D-05 / SPEC REVISION R-01: por decisión del usuario, cargar conserva la
  presentación actual `RESTORED` en ambos jugadores. Se eliminan de la cabecera
  las siete filas y `move_line_count`; no se amplía RESTORE para transmitirlos.
- D-06: 2026-09-16, el plan STJ v2 (tmp-con-cabecera, records 16/24 B, LOAD
  reconstruye filas, coexistencia con el provisional de 60 B) queda rechazado.
  Se ejecuta este dossier. Wrappers de seek no entran en CONFIG (15 B libres).
- D-07: 2026-09-18, append v1 = manejador persistente. Reopen/seek rechazado
  en v1. No fusionar `spike/journal-m0` (SP 352). M1 debe caber en SP ≥512 y
  BSS nuevo ≤8 B; el gancho T-01 (~168 B) es ilegal en producto.
- D-08: 2026-09-19, Classic M2. Revisa D-02 solo para leer (revocado por
  D-09): LOAD acepta el `.STJ` de 60 bytes como snapshot con historial
  parcial. Next/SpectraNext siguen en el snapshot de 60 bytes y sin gancho de
  diario.
  - Append = SAVELOAD entrada 3 en ASM (`journal_append.asm`); LOAD/SAVE =
    overlay JOURNAL. El trampolín residente pasa el texto de la jugada, así
    que MOVE registra la promoción (bits 12..14).
  - Estado residente, 3 bytes: `_spectrum_journal_fd` (handle+1 del TMP;
    0 = crear en el próximo ply; `0x3F` = append desactivado tras un fallo de
    escritura, el TMP conserva el prefijo; bit 6 = parcial) y
    `_spectrum_journal_pend` (bit 7 armado, bit 6 parcial, bit 0 cuerpo en
    `SHATJRNL.NEW`) y `_spectrum_journal_warning` (0 libre, 1 pendiente,
    2 mostrado). Contrato completo en `asm/spectrum/journal.asm`.
  - GAME START escribe `pend = 0x80`; un RESTORE remoto `0xC0`. LOAD nunca
    toca el historial vivo: valida, copia el cuerpo a `SHATJRNL.NEW` y deja
    `pend` sin armar. Aplicar el snapshot localmente lo arma
    (`saveload_apply_snapshot`). Una carga no aplicada (color, decode,
    rechazo o cancelación del peer) queda inerte, sin código residente en
    esas rutas. Si LOAD encuentra un `pend` armado, lo resuelve antes de
    sobrescribirlo, para que el orden GAME START/LOAD no se pierda.
  - SAVE lee la historia que nombra `pend` (armado) o `fd`, no modifica
    ninguno de los dos y deja el handle vivo en EOF en toda salida. Un
    registro partido al final se descarta y marca parcial.
  - El TMP y `.NEW` se crean con FA_READ|FA_WRITE|FA_CREATE_AL (`0x0F`) vía
    `esx_journal.asm`; CONFIG conserva `0x0E`. `esx_fseek_set`/`esx_fsync` no
    entran en CONFIG.
  - El sello 8.3 del SAVE aplica `fat_day_delta` como `saveload_write_stamp`.
  - Vectores: `run_journal_vectors` ejecuta el ASM real contra un esxDOS
    simulado que exige FA_READ para leer.
- D-09: 2026-09-19, LOAD solo acepta `.STJ` v1 completo. Revoca la excepción
  de lectura de 60 bytes de D-08 en Classic y fija el contrato para Next y
  SpectraNext: un fichero de exactamente 60 bytes, un cuerpo truncado, un byte
  sobrante, CRC inválidos o cabecera/magic/versión inválidos devuelven
  LOAD_FAIL sin tocar instantánea, `fd`, `pend`, TMP ni `SHATJRNL.NEW`. La
  validación del cuerpo completo precede a cualquier mutación; la copia al
  temporal ocurre después. SAVE siempre escribe STJ1.
- D-10: 2026-09-19, primitivas XFS promovidas en la autoridad SpectraNext
  (rama `xfs-update-open`, publicada): `esx_fopen_update` (`273075f`) y
  `esx_fseek_end` (`a6b5c88`). El adapter conserva un único handle, cierra e
  invalida el predecesor y publica `fd + 1`; el append SPXN usa
  reopen-update + SEEK_END + write + close, sin descriptor persistente. La
  validación de longitud de LOAD puede usar el tamaño devuelto por SEEK_END.
  Coste medido: +52 B por overlay que enlaza el adapter.
- D-11: 2026-09-19, SpectraNext S2 escribe siempre a
  `/CFG/SHATSAVE.TMP`, confirma ese temporal y lo promueve con RENAME; nunca
  trunca primero el destino válido. La autoridad publica `esx_frename`
  (`317bcfe`). LOAD obtiene la longitud exacta con SEEK_END, valida cabecera,
  `76 + 2*n`, CRC y EOF antes de copiar snapshot o alterar el diario. Los
  bloques de LOAD, SAVE y staging reabren ruta+offset y mantienen un solo
  handle XFS. El borrado renombra cada slot a su propio `.DEL`: la lápida no
  libera flash, pero otro borrado del mismo slot reemplaza ese mismo destino.
- D-12: 2026-09-20, las rutas usadas por el codec y el append SpectraNext, y el
  registro de 2 bytes que se va a escribir, se copian al scratch fijo fuera del
  overlay antes de cada llamada XFS. La ROM XFS pagina el cartucho sobre
  `0x0000..0x3fff`, incluido SAVELOAD en Page B desde `0x2000`; por tanto, una
  ruta o payload residente en el propio overlay deja de ser legible durante la
  llamada. No cambia ABI, BSS ni formato STJ.
- D-13: 2026-09-20, LOAD rechaza `record_count` cuyo cuerpo desborda 16 bits y
  cualquier registro que no sea MOVE con promoción 0..4 o el control exacto
  `0x8000`. SAVE reabre y valida tamaño, cabecera, registros y CRC del fichero
  escrito antes de informar éxito; si falla, elimina el candidato sin adoptar
  un diario corrupto. Se conserva el formato STJ1 y el ABI.
- D-14: 2026-09-22, reconciliación del replay Classic añadido en `9f440df`:
  reemplaza la presentación local descrita en D-05, sin modificar STJ1 ni
  RESTORE. Solo BODY completo puede partir de ply cero; un cuerpo parcial o
  ilegible muestra `RESTORED` según el turno del snapshot. El replay no altera
  `fd`, `pend`, el cuerpo persistido ni el tablero.
  - La adopción NEW→TMP y el sondeo de EOF comprueban Carry: un error de
    lectura no es fin de fichero. Una adopción fallida conserva el prefijo
    parcial y desactiva append; no borra el NEW original como si hubiese
    terminado correctamente.
  - Un `F_SYNC` fallido no informa éxito. Si falla el cierre de la copia de
    staging, la posición sigue cargándose y la continuación queda parcial.
  - Los vectores ejecutan el ASM productivo con errores de read/sync/close,
    ambos lados de `RESTORED` y comprobación de handles/estado. El ejecutor
    Python invoca `main` después de definir todos los vectores.

### Ledger de la corrección D-14

| Opción | Evidencia y coste | Riesgo / verificación | Decisión |
|---|---|---|---|
| Comprobar Carry y cierre reutilizando salidas de error | Vectores reproducen adopción truncada y falso éxito; sin estado residente nuevo | Cierre del handle correcto, prefijo parcial; vectores Z80 + límites enlazados | **Hacer** |
| Replay parcial o ilegible → marcador según snapshot | El cuerpo parcial no determina su ply inicial; evita atribuir una jugada negra a blancas | Conservar cuerpo, `fd` y `pend`; vectores ctx=1/3 | **Hacer** |
| Registrar el import XFS y reconciliar baselines con el replay ya integrado | Faltaban `spxn_rom_error` en capacidades y el getter de visibilidad en ambos manifiestos; el baseline de tamaño era anterior al replay/ZX0 | Import específico, ABI existente y artefactos del toolchain fijado; conservar pisos de pila y caps | **Hacer** |
| Inferir el inicio del historial parcial o añadir checkpoints | El prefijo también puede terminar antes del snapshot tras fallo de escritura | Requiere ampliar formato/estado, sin resolver la evidencia ausente | **Rechazar** |
| Recuperación automática tras corte y port Qt M3 | Fuera de estos fixes; el temporal sigue sin snapshot | Requiere su propio hito y validación | **Diferir** |

### Corrección D-15: recuperación y fallos del gancho (2026-09-26)

- El gancho comprueba el resultado del dispatcher. Si falta un MOVE/UNDO,
  marca parcial tanto el diario vivo como la sustitución armada; SAVE no
  puede declarar completa una secuencia a la que le falta ese evento. Los
  fallos de un repaint (ctx 1/3) no degradan el cuerpo persistido.
- Un byte residente limita el aviso a una vez por historial aplicado. Se
  reinicia al comenzar una partida o aplicar una carga, nunca al preparar
  una carga que pueda rechazarse. El siguiente ciclo de eventos muestra
  `History incomplete` mediante la GUI residente, después del aviso de
  turno y sin necesitar otra lectura de la SD.
- Una carga local aplica `view_flags`, tanto offline como tras aprobación
  del peer. Un RESTORE entrante conserva la orientación del receptor.
- XFS crea/trunca TMP una sola vez al iniciar NEW→TMP, incluido el cuerpo
  vacío. Los bloques posteriores abren para actualizar; un fallo no vuelve
  a crear TMP. Se consulta el error de lectura antes de que el cierre lo
  borre y se comprueban los cierres. NEW solo se borra tras copiar todo.
- Classic recorre el cuerpo dos veces: calcula el ply final y pinta una
  ventana fija de siete filas. Así los MOVE transitorios no expulsan filas
  que un UNDO posterior vuelve a necesitar. Usa tres bytes locales del
  overlay y ningún byte residente adicional; duplica las lecturas del
  replay y añade una apertura/cierre de NEW. Un fallo en cualquiera de las
  pasadas descarta la lista reconstruida y muestra `RESTORED`.

| Opción | Evidencia / coste esperado | Riesgo / verificación | Decisión |
|---|---|---|---|
| Resultado del dispatcher + estado parcial | Falta reproducida entre dos MOVE; estado fd/pend existente | Vectores gancho + codec: primer MOVE, MOVE intermedio, UNDO, LOAD y repaint | **Hacer** |
| Aviso residente con un byte de estado | La SD ausente impide cargar GUI_LOG; memoria constante | ABI de GUI y pila; repetición, nuevo historial y recuperación | **Hacer** |
| Orientación explícita al aplicar snapshot | El guardado ya contiene view_flags | Ambos colores/orientaciones, carga local con/sin peer y RESTORE remoto | **Hacer** |
| Truncado inicial y comprobación XFS | UPDATE no trunca; CLOSE borra el error de READ | Temporal previo más largo, vacío y fallos de open/read/write/close | **Hacer** |
| Replay con RAM acotada | Una fila expulsada no puede reconstruirse con el scroll actual | Secuencias MOVE/UNDO y ventana final de siete filas | **Hacer** |
| Especializar el snapshot privado y reutilizar notify_error | El primer enlace solapaba el vector IM2 por 27 B; todos los callers usan restore_snapshot | Regresiones de orientación/gancho y enlace Classic; ahorro medido de 29 B frente al primer intento | **Hacer** |
| Registrar el coste funcional en el baseline Classic | Con z88dk 762578732, +79 B residente y +82 B SAVELOAD; únicos aumentos, BSS y pisos sin cambios | Límite IM2 intacto, 2 B libres antes del vector, SP gap 599 B y SAVELOAD 1655/2048 | **Hacer**, coste explícito de los fixes |
| Cambiar STJ1, RESTORE o recuperación tras corte | No es necesario para corregir los defectos | Requiere otro contrato y validación | **Diferir** |

### D-16: enlace con la recuperación de partida (2026-09-28)

- El residente añade 4 B (`_spectrum_journal_count`, `_spectrum_journal_crc`)
  que describen el prefijo durable de `SHATJRNL.TMP`: se reinician al crear
  TMP, se acumulan al adoptar NEW y suman cada registro tras `F_SYNC`. Un
  fallo los deja en el último prefijo durable.
- La llamada de aplicación de una instantánea (ctx 1/3) consume ya el
  interruptor armado: Classic repinta desde NEW y ambos targets adoptan NEW o
  crean un TMP vacío. Antes lo consumía el primer MOVE. Así el checkpoint de
  recuperación que sigue a LOAD/RESTORE describe el TMP vivo. SpectraNext
  hace lo mismo y además hace commit de `SHATJRNL.TMP` al crearlo, para que
  el diario pase a la flash del cartucho y sobreviva al apagado (antes vivía
  en su RAM); cada jugada escribe entonces en flash.
- Un fallo al crear TMP en ese consumo publica el aviso `History incomplete`
  (`fd` = `0x40`) en lugar de quedar silencioso.
- SpectraNext: un `READ` al final del fichero termina en hardware con carry y
  EOF (`$21`, SpectraNext `docs/porting.md`). La adopción de NEW y
  `stj_io_read_any` lo tratan ya como la lectura de 0 B de esxDOS; antes la
  adopción y los bucles STJ que leen hasta 0 B habrían fallado en hardware.
  Un TMP existente se trunca con `O_WRONLY|O_TRUNC`, sin `O_CREAT`.
- STJ1, el registro de 16 bits y la profundidad de TAKEBACK no cambian.
  Detalle y costes: [game-recovery-experiment.md](game-recovery-experiment.md).

## Verificación y siguiente acción

- D-15 (2026-09-26), cierre sobre `7f80176` más estos fixes, con z88dk
  `762578732`: regresiones host/ASM, ocho escenarios del gancho residente,
  DIRECT y MQTT 50/60 Hz, manifiestos ABI Classic/Next, 265 casos BOARD por
  target, editor enlazado e IY con interrupciones: **PASS**. Se reutilizan
  los prerrequisitos de la primera invocación de `full-check`; sus fallos
  de límites de sesión, solape IM2 y crecimiento presupuestado se resuelven
  con los checks específicos, sin repetir la suite global. Los manifiestos
  ABI y los límites duros no cambian. El baseline Classic registra únicamente
  el coste funcional medido descrito en el ledger.

  | Target | Residente | BSS | SP gap | SAVELOAD |
  |---|---:|---:|---:|---:|
  | Classic | 35676 B | 419 B | 599 B | 1655/2048 B |
  | Next NEX | 32647 B | 517 B | 3532 B | 2863/4096 B |
  | SpectraNext | 35766 B | 269 B | 909 B | 3910/4096 B |

  Classic conserva solo 2 B libres antes del vector IM2 fijo, con el mismo
  SP gap de 599 B que el baseline anterior. SpectraNext se enlaza contra
  la autoridad `4282134`. Evidencia local: `build/journal-fixes-check.log`,
  `build/journal-fixes-close.log`, `build/journal-fixes-*.log` y los tres
  `size_report.json`. Pendiente físico: retirar/fallar SD durante append,
  LOAD local con ambas orientaciones, adopción sobre TMP anterior más largo
  y replay de siete filas tras UNDO. No se añade recuperación tras corte.
  Ejecución: dos workers de implementación y tres encargos en total;
  modelo/effort solicitados `gpt-5.6-sol/high`, no confirmados por el runtime.
- Shrink posterior a D-14 (2026-09-22), con el mismo toolchain: se aplican
  los seis candidatos del scan sin cambiar formato, validación ni recuperación.
  CONFIG ahorra 63 B por target al especializar su lector privado; el codec
  ahorra 30 B compartiendo cierres y 16 B eliminando el CRC de staging que no
  se consumía. JOURNAL Classic queda en 1923/2048 y SAVELOAD Next en 2868/4096;
  CONFIG queda en 1970/2048 y 2018/4096 respectivamente.
  GUI ahorra 37 B compartiendo cierre de menú, 13 B CODE y 1 B BSS con el
  contador de avisos de ocho bits, y otros 17 B Classic reutilizando paneles
  (dos bytes más de pila dentro del helper). La reserva IM2 se separa en una
  sección fija de dos bytes, inicializada antes de activar IM2, y elimina
  16 B de relleno CODE. El BSS ordinario sigue cargándose a cero desde el TAP.
  Classic: residente 35680→35597, BSS 420→419 y SP gap 596→599; quedan 81 B
  entre el BSS ordinario y el vector fijo, no atribuibles al margen de pila.
  Next: residente 32618→32568, BSS 518→517 y SP gap 3560→3611.
  `make full-check` pasa en una invocación, además de CONFIG XFS host; ABI,
  TAP, límites, 265 casos BOARD por target, editor e IY enlazados verdes.
  Los avisos se comprueban en los ticks 249/250/251 y las transiciones GUI
  a 50/60 Hz. Evidencia local: `build/shrink_size_deltas.json` y
  `build/shrink-full-check.log`. Firmware SpectraNext y prueba física pendientes.
- D-14: 19 escenarios ASM nuevos de fallos y suite host verdes; cierre
  equivalente a `full-check` verde reutilizando los prerrequisitos ya pasados
  tras reconciliar los baselines: módulos, ABI Classic/Next, límites de tamaño,
  265 casos BOARD por target, editor enlazado e IY con interrupciones.
  Coste medido en el mismo
  arnés antes/después: codec 1762→1773 B (+11), append/replay 912→936 B (+24),
  sin estado residente adicional. El cierre enlazado con z88dk `762578732c`
  deja Classic en 35680 B residente, 420 BSS, SP gap 596, SAVELOAD 1573/2048 y
  JOURNAL 1969/2048; Next en 32618 B residente, 518 BSS, SP gap 3560 y SAVELOAD
  2914/4096. Las diferencias respecto al baseline anterior también incluyen
  cambios previos: replay, límite de salida ZX0 y reducción de SETUP/residente;
  no se atribuyen a estos fixes. El baseline se reconcilia con ese árbol y los
  manifiestos solo añaden el import de visibilidad ya usado por el replay.
  Pendiente físico: 3 MOVE + TAKEBACK, SAVE n=4, LOAD + MOVE + SAVE n=5;
  RESTORE remoto seguido de SAVE/LOAD debe mostrar `RESTORED`, sin inventar
  filas anteriores. SpectraNext no se recompila en esta auditoría.
- Revisión documental: hecha 2026-09-16 (cinco auditorías + revisión Codex).
- Hardware M0 Classic: T-00 y T-01 DIRECT persistente hechos. MQTT/Next
  residuales de primera pasada M1, no de otra sonda M0.
- E-06/E-07 ya no bloquean la estrategia. E-08 bloquea copiar el spike.
- M1 persist Classic DIRECT verificado en hardware (DUMPJRNL).
- M2 Classic en software (D-08, D-09, D-13); JOURNAL enlazado 1944/2048,
  residente 35763 y SP gap 512. Vectores cubren el rechazo de un fichero de
  60 B, cuerpo truncado, byte sobrante, CRC inválido, cabecera, cuenta o
  registro inválidos, y corrupción posterior a SAVE, sin modificar
  instantánea, `fd`, `pend` ni TMP. Pendiente en DivMMC: `F_READ` sobre el TMP
  `0x0F`, `F_SYNC`, `F_SEEK` y el rechazo de un fichero real de 60 B. Prueba
  mínima: 3 jugadas + TAKEBACK, SAVE, `.STJ` de 84 bytes con n=4; LOAD,
  primera jugada, SAVE con n=5.
- M2 Next en software y ABI (E-09): `JOURNAL_HOOK=1 STJ_SAVELOAD=1`; el NEX no
  empaqueta JOURNAL y enlaza el codec compartido en SAVELOAD entradas 0/1 con
  el alias `stj_saveload.asm`; SAVELOAD queda en 2879/4096 y el SP gap en 3494.
  `make full-check` verde en una re-ejecución
  única tras corregir el agregado `OVL_FILE` del baseline (`module-guards`,
  `test`, ABI Classic+Next, `size-check --fail-on-growth`, `nex-size-report` y
  comprobaciones enlazadas). Pendiente de hardware: SAVE de 84 B con n=4, LOAD
  + jugada + SAVE n=5, intercambio Classic↔Next en ambos sentidos, LOAD de
  60 B rechazado y AC-T-04 con 100 plies.
- SpectraNext S1 (D-10, E-10): `tap-spectranext` activa `JOURNAL_HOOK=1` y
  `ATLAS_EXTRA=TIME`; el append vive en SAVELOAD y usa
  `esx_fopen_update` + `esx_fseek_end` + write + close, con adopción de
  `SHATJRNL.NEW` por bloques y un solo handle vivo. LOAD/SAVE siguen siendo el
  C de 60 B: S1 fue solo un checkpoint interno.
- SpectraNext S2 (D-11, E-11): `STJ_SAVELOAD=1` elimina LOAD/SAVE C y enlaza
  el codec compartido en SAVELOAD. El TAP, lowmem, `module-guards`, matriz de
  composición y vectores ASM Classic/Next están verdes. D-12 corrige el fallo
  de rutas y payload paginados; D-13 añade la revalidación anterior al commit;
  SAVELOAD queda en 3887/4096 y el SP gap en 870.
  Falta hardware SPXN:
  SAVE 84 B n=4; LOAD + jugada + SAVE n=5; rechazo de 60 B sin mutación;
  fallo inyectado antes de RENAME preserva el destino; 100 plies con red viva.
  TIME permanece id 17 y no existe overlay JOURNAL.
- `tests/tools/test_spectranext_build.py::test_busy_reply...` falla ya en HEAD
  (pre-existente, no relacionado); solo lo ejecuta `spectranext-network-test`,
  no `make test`/`full-check`.
- Siguiente acción: hardware Classic, Next y SpectraNext; después, M3 (Qt).
