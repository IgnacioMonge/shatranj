# Recuperación de partida

Estado: implementada el 2026-09-28 e integrada en `main` con `85231782`
(2026-09-29), sobre Classic/esxDOS, Next NEX y SpectraNext (XFS). Qt no la
ofrece; como invitado recibe el RESTORE de siempre. El seguimiento vivo de la
validación pendiente está en [handoff.md](handoff.md).
Las secciones marcadas «Archivo» conservan el plan del 2026-09-26 y el
prototipo `/recover`, ambos sustituidos por lo que sigue.

## Diagnóstico del intento anterior

El plan de 2026-09-26 metía en el residente de sesión un protocolo de
transacciones propio (verbos `GC1` a `NE1`, generaciones, decisiones en
`SHATJRNL.NEW` y eventos de 73 B con CRC encadenado). Ese coste residente no
cabía: faltaban 7 KiB en Classic y 4,5 KiB en Next, pila incluida. El producto
ya tenía las dos piezas necesarias:

1. **LOAD/RESTORE con consentimiento**, con reintentos acotados,
   duplicados idempotentes, `RN` ante rechazo o expiración y aplicación en el
   invitado antes de `RA`.
2. **Checkpoint por jugada en overlay** (dos ficheros alternos con CRC) y el
   diario `SHATJRNL.TMP` con su adopción `NEW → TMP`.

La recuperación reutiliza ambas piezas. El residente solo decide cuándo
ofrecerla; todo el trabajo de ficheros vive en overlays.

## Comportamiento

- **Oferta.** Cuando el rival queda listo (DIRECT `HELLO` o presencia MQTT),
  y antes de cualquier GAME START o reloj, el host sondea su checkpoint más
  reciente. Si es ofrecible muestra `Resume last game? Y/N`. Solo el host
  ofrece, una vez por conexión; reconectar la repite.
- **Ofrecible** significa: checkpoint válido más reciente, escrito por un
  host que jugaba el mismo color que el actual, con ply > 0 y partida no
  terminada. El checkpoint más antiguo nunca sustituye a uno más nuevo no
  ofrecible.
- **Y** entra en el camino de FILE LOAD: RQ, aviso normal de RESTORE en el
  invitado como consentimiento, transferencia y aplicación en ambos. El
  invitado recibe posición, flags, ply y relojes GAME/TURN; su historia
  empieza parcial (`RESTORED`). El host recupera además su historia.
- **N** no crea partida ni toca ficheros. El rechazo del invitado tras **Y**
  deja intactos `SHATJRNL.TMP` y los checkpoints; solo queda sustituido el
  staging `SHATJRNL.NEW`, sin armar, como tras un FILE LOAD rechazado.
  Pulsar START escribe un checkpoint de ply 0 que retira la oferta.
- **Salida elegida**: la recuperación es para interrupciones. Si cualquiera
  de los dos jugadores abandona una partida viva con la desconexión del menú,
  su Spectrum, y el del rival al recibir su `BYE`, escriben antes un
  checkpoint de la posición exacta marcado OVER; como es el más reciente y
  nunca se ofrece, retira la oferta. Lo hace RECOVERY (entrada 3, `RETIRE`),
  que comprueba que haya partida viva; el residente solo la invoca, porque
  Classic no tiene margen. Una conexión perdida, un reset o un corte de
  alimentación no escriben nada y la oferta sigue en pie. Por MQTT, la
  pérdida de PING genera un `BYE` lógico sin texto, que tampoco retira.
  Esperar al rival y cancelar no la retira: es el caso de un rival caído. El
  `BYE` que responde a un `HELLO` DIRECT malformado sí la retiraría; en
  partida solo ocurre con un par defectuoso. Si esa escritura falla no se
  avisa: la consecuencia es una oferta que se puede rechazar.
- **Relojes**: los del último checkpoint. No se suma el tiempo desconectado.
- **Final de partida** (mate, ahogado, tablas, abandono): se escribe un
  checkpoint OVER que nunca se ofrece, de modo que una partida terminada no
  reaparece como viva.

## Formato SRC2

Dos ficheros, `/SYS/CONFIG/SHATRCV0.RCV` y `SHATRCV1.RCV`, de 73 B exactos.
Enteros little-endian, CRC-16/CCITT-FALSE con inicio `0xFFFF`.

| Offset | Tamaño | Contenido |
|---:|---:|---|
| 0 | 4 | `SRC2` |
| 4 | 2 | Generación, uint16 modular; gana el más nuevo de dos válidos |
| 6 | 60 | Instantánea RESTORE Base64URL (tablero, turno, ply, flags, relojes) |
| 66 | 2 | Registros de `SHATJRNL.TMP` que la instantánea extiende |
| 68 | 2 | CRC de esos `2 × registros` bytes |
| 70 | 1 | Flags: bit 0 historia parcial, 1 sin enlace, 2 escrito por host, 3 escritor negro, 4 reanudable |
| 71 | 2 | CRC de los bytes 0..70 |

Cada escritura apunta a la copia que no es la más reciente y solo informa
éxito tras escribir, sincronizar, cerrar y releer el registro exacto. Un error
de E/S al examinar cualquiera de las dos aborta antes de truncar. Los `SRC1`
del prototipo no son válidos y se sustituyen en la primera escritura.

Classic y Next solo examinan las dos copias en la primera escritura de cada
arranque, o tras una escritura fallida. Desde la siguiente, 3 B residentes
(`_spectrum_rcv_cache`) recuerdan qué copia escribió y verificó la última y
con qué generación. Así cada checkpoint se ahorra 2 aperturas y 4 lecturas. A
cambio, una SD cambiada o editada sin reiniciar no se detecta. SpectraNext
sigue examinando siempre.

## Enlace con el diario

El residente guarda 4 B: `_spectrum_journal_count` y `_spectrum_journal_crc`,
que describen el prefijo de `SHATJRNL.TMP` ya durable. El append (SAVELOAD
entrada 3) los reinicia antes de crear TMP, los acumula al adoptar NEW y suma
cada registro sincronizado. Un fallo los deja en el último prefijo durable. El
checkpoint los copia al registro.

Al aplicar una instantánea (LOAD, RESTORE o recuperación), la llamada de
aplicación (`HL = 1/3`) consume ya el interruptor armado: Classic y Next
primero repintan la lista desde NEW en esa misma llamada; SpectraNext la
repinta justo antes desde RECOVERY (ver «Lista de jugadas al cargar o
reanudar»). Después las tres adoptan NEW o crean un TMP vacío. La adopción sincroniza TMP antes de borrar NEW, porque el primer
registro puede tardar minutos; si la sincronización falla, NEW se conserva y
el diario queda parcial y deshabilitado con aviso. Así el checkpoint que sigue
ya describe el TMP vivo. Mientras GAME
START tiene el interruptor armado antes del primer ply, el checkpoint se marca
sin enlace.

Al aceptar, el stage (CONTROL entrada 2) cierra el handle vivo, copia la
instantánea y lleva a `SHATJRNL.NEW` exactamente `2 × registros` bytes de TMP,
calculando su CRC. Deja `pend` sin armar: `0x01` historia completa, `0x41`
parcial, `0x40` sin cuerpo verificable. La aplicación consume ese `pend` como
un LOAD; las tres plataformas reconstruyen las filas. Un CRC distinto, un TMP más corto o
cualquier error de E/S degradan a `RESTORED` sin impedir la carga.

Semántica de corte, jugada a jugada: el registro del diario precede al
checkpoint. Un corte entre ambos recupera el ply anterior, con su prefijo; un
corte durante el checkpoint deja la otra copia válida, cuyo prefijo sigue en
TMP porque este solo crece dentro de una partida. Un corte durante la adopción
de una carga (TMP ya truncado, NEW intacto) deja el último checkpoint sin
prefijo verificable: se ofrece la posición con `RESTORED` y la historia de esa
carga se pierde, porque el sondeo no consulta NEW. CRC y relectura no prueban
atomicidad física del sector ni integridad de FAT.

## Lista de jugadas al cargar o reanudar

Tras un FILE LOAD o una reanudación, la lista se repinta desde
`SHATJRNL.NEW` antes de que la aplicación lo adopte. El código es uno solo,
`asm/overlay/journal/journal_replay.inc`: si `pend` indica cuerpo completo
(bit 0 activo, bit 6 limpio), una primera pasada cuenta los plies aplicando
los UNDO y una segunda pinta solo la ventana visible (siete filas, los
últimos 13 o 14 plies). Un cuerpo parcial, ausente, de longitud impar o con
cualquier error de apertura, lectura o cierre pinta solo `RESTORED` en la
columna del último bando que movió (ctx 3 = ply impar, blancas; 1 = par,
negras). Entre registros se atiende la red.

- **Classic y Next**: dentro de la llamada de aplicación de SAVELOAD
  (entrada 3), con el handle esxDOS abierto y bloques de 160 B. Next lee en
  `ja_io_buf` y usa el mismo lector que la adopción. Antes Next pintaba
  siempre `RESTORED`.
- **SpectraNext**: SAVELOAD está a 27 B del límite, así que el replay es la
  entrada 2 de RECOVERY (`rcv_replay.asm`), que ya enlaza el adaptador XFS.
  `app.c` la llama y luego consume el interruptor con `HL = 1`; si el overlay
  no puede ejecutarse (ctx[4] sigue en `0xFF`), GUI_LOG pinta `RESTORED`.
  Como el adaptador tiene un solo handle y la red se atiende entre
  registros, cada bloque de 128 B abre NEW, posiciona con `spxn_xfs_fseek`,
  lee y cierra; la ruta se copia al final del scratch. EOF es `READ` con
  error `$21` y 0 B. Classic y Next conservan un stub `ret` en esa entrada.

## Next: buffers de esxDOS por encima de 0x3FFF

La primera prueba en hardware Next (2026-09-28) dio `Recovery failed` en cada
START y `LOAD FAILED` tras un FILE SAVE. Causa: cada `RST 8` de Next pone
MMU1 en ROM (`NEXTREG 0x51, 0xFF`), así que el scratch de overlays en
`0x3C78` no es memoria del programa durante la llamada. Checkpoints, stage,
adopción de NEW y el códec STJ le pasaban ese scratch a esxDOS como buffer.
Ahora RECOVERY y CONTROL usan `rcv_io` (160 B en la memoria del overlay,
`rcv_store.asm`) y SAVELOAD `ja_io_buf` (160 B, compartido por el append y el
códec STJ). Los dos van en `code_user`: z80asm resuelve con base errónea un
`EQU` hacia una etiqueta de otra sección, y el vector Next de
`test_recovery.py` lo detectó. El modelo esxDOS de ese test rechaza cualquier
ruta o buffer por debajo de `0x4000`, y `check_lowmem_layout.py` exige el
buffer de SAVELOAD en Next.

## SpectraNext (XFS)

El formato, las reglas de oferta y el enlace son los mismos; cambian los
ficheros y cómo se hacen durables.

- **Rutas**: `/CFG/SHATRCV0.RCV`, `/CFG/SHATRCV1.RCV`, temporal
  `/CFG/SHATRCV.NEW`; el diario sigue en `/CFG/SHATJRNL.TMP` y `.NEW`.
- **Durabilidad**: un fichero nuevo de XFS vive en la RAM del cartucho, que se
  borra al apagar, hasta su primer commit; una ruta ya en flash se sobrescribe
  sin volver a hacer commit. Cada checkpoint se escribe con
  `spxf_replace_atomic()` (temporal, commit y `RENAME` sobre la ranura), el
  mismo reemplazo atómico que usa la configuración, y se relee. Si un corte
  entre el commit y el `RENAME` del helper dejó el temporal comprometido, el
  helper ya no puede sustituirlo (UNLINK da EIO y el segundo commit falla);
  el escritor aplica entonces la receta de SpectraNext para ese resto:
  reescribirlo con `O_WRONLY|O_TRUNC`, sin commit, y renombrarlo sobre la
  ranura. El diario abre un `SHATJRNL.TMP` existente con `O_WRONLY|O_TRUNC`
  (nunca `O_CREAT` sobre una ruta en flash); si no existe, rehace el `MKDIR` y
  lo crea. Después hace commit e ignora el resultado: sobre una ruta ya en
  flash devuelve EIO, que la prueba física de SpectraNext (`docs/status.md`,
  open-update proof) registra como esperado e inocuo, y así un TMP que quedó
  en RAM también pasa a flash. Sin ese commit, tras un apagado el TMP no
  existiría y la oferta degradaría siempre a `RESTORED`. La misma prueba
  física confirma el append de 2 B sobre un fichero en flash.
- **Fin de fichero**: en hardware un `READ` al final del fichero termina con
  carry y error EOF (`$21`). El escaneo de ranuras pide 74 B en una sola
  lectura para fijar el tamaño exacto sin sondear el EOF, y los lectores
  compartidos (stage, adopción de NEW y `stj_io_read_any`) tratan EOF como la
  lectura de 0 B de esxDOS. Esto corrige también un defecto previo de
  SpectraNext: la adopción de `SHATJRNL.NEW` y los bucles STJ que leen hasta
  0 B habrían fallado en hardware con EOF.
- **Un solo handle**: el adaptador XFS mantiene un fichero abierto. El stage
  de la reanudación copia TMP a NEW en bloques de 128 B reabriendo cada
  fichero y posicionándose con `spxn_xfs_fseek`; el diario no guarda handle
  vivo. Los errores se leen con `spxn_rom_error()` (`ENOENT` = 2) antes del
  cierre que los borra.
- **Rutas en RAM baja**: XFS pagina su ROM sobre Page B, donde corre el
  overlay, así que cada ruta se copia antes al scratch (`RCV_TMP` o el final
  del bloque de copia).
- **Consumo al aplicar**: SpectraNext también consume el interruptor en la
  llamada de aplicación (ctx 1/3), como Classic y Next.
- **Paquete del instalador**: el overlay RECOVERY lleva `SHATRANJ.OVL` a
  39.496 B y el paquete a unos 50 KB. El instalador SPXI v1 de SpectraNext no
  lo admitía (máximo 37.470 B por fichero expandido y 48.128 B de paquete);
  `make spectranext-port-build` necesita el instalador SPXI v2 de SpectraNext
  (ya en `main`), con el que genera un `SHATRANJ.PKG` de 50.961 B que ocupa
  13 páginas de SRAM.
- **Overlays**: TIME conserva el id 17 y RECOVERY pasa a ser el 18
  (`ATLAS_EXTRA='TIME RECOVERY'`). El sondeo y el stage van en CONTROL.

## Decisiones

| Opción | Evidencia y coste | Decisión |
|---|---|---|
| Reutilizar LOAD/RESTORE como transacción | Ya en producción, sin verbos nuevos; interoperable con Qt | Hacer |
| Dos RCV enlazados al TMP por cuenta y CRC | Coste constante por jugada; el TMP sigue siendo la única historia | Hacer (aprobado 2026-09-28) |
| Consumir el interruptor al aplicar | Evita un checkpoint sin enlace entre carga y primer ply | Hacer |
| Sondeo y stage en CONTROL | RECOVERY estaba a 2028/2048 B; CONTROL tenía 1 KiB libre; sin overlay ni página Next nuevos | Hacer |
| Flags del checkpoint en ASM | z88dk SDCC perdió los stores de una cadena `if (...) x |= k` sobre un local; el C solo decide «reanudable» | Hacer |
| Fallo del checkpoint de START | Aviso `Recovery failed` y se sigue jugando; tras un corte podría ofrecerse la anterior (respuesta N). El aviso es único por partida: si falla también el checkpoint OVER, la partida terminada puede ofrecerse sin nuevo aviso | Aprobado 2026-09-28 |
| No ofrecer partidas terminadas | Un OVER ofrecido como «Resume» resucitaría una partida abandonada | Hacer |
| SpectraNext: checkpoint con `spxf_replace_atomic()` | Primitiva ya usada por la configuración; deja la ranura en flash sin depender de un commit por fichero | Hacer |
| SpectraNext: commit de `SHATJRNL.TMP` al crearlo | Sin él el diario vive en RAM del cartucho y un apagado lo borra; a cambio cada jugada escribe en flash (latencia y desgaste por medir en hardware) | Hacer |
| Barrera `GC1`–`NE1`, TMP único con eventos de 73 B | ~7 KiB residentes | Retirado |

Corrección incidental: el catálogo `GUI_LOG` rechazaba el id 46
(`CONFIG I/O ERROR`) porque su cuenta era 46; ahora es 48 y el vector lo
comprueba.

Una build con `ATLAS_EXTRA=RECOVERY` pero sin gancho de diario no enlaza el
sondeo en CONTROL: su entrada 2 responde «sin candidato» y nunca ofrece
reanudar.

## Coste enlazado

Medido el 2026-09-28 con `make abi-baseline size-baseline nex-size-report`.
`bb3dd7f` no tiene recuperación; `2f8414b` es el prototipo `/recover`.

| Métrica | `bb3dd7f` | Prototipo | Esta rama |
|---|---:|---:|---:|
| Classic residente | 35.387 B | 35.676 B | 35.679 B |
| Classic BSS (informe) | — | 419 B | 413 B |
| Classic fin de BSS (vector IM2 en `0xFCFF`) | — | `0xFCFD` | `0xFCFA` |
| Classic SP_GAP (suelo 512) | 599 B | 599 B | 599 B |
| Classic TAP | 36.055 B | 36.336 B | 36.348 B |
| Classic RECOVERY / CONTROL | — / — | 2.028 / 1.034 B | 2.010 / 1.957 B |
| Classic SAVELOAD / GUI_LOG | — | 1.655 / 1.863 B | 1.770 / 1.881 B |
| Next residente | 32.529 B | 32.816 B | 32.813 B |
| Next SP_GAP (suelo 3.072) | 3.657 B | 3.369 B | 3.372 B |
| Next SAVELOAD / CONTROL / RECOVERY | — | — / — / 2.084 B | 3.746 / 2.165 / 2.226 B |

«—»: no medido o inexistente. Los overlays Classic tienen 2.048 B y los Next
4.096 B; en Next cada uno de esos tres overlays lleva 160 B de búfer propio
(ver «Next: buffers de esxDOS por encima de 0x3FFF»); de lo que crece SAVELOAD, 11 B son el sync de la adopción y 591 B el replay de la lista. RECOVERY suma 124 B en Classic y Next: el stub de la entrada 2 y `RETIRE` (entrada 3). Frente al
prototipo, que no ofrecía nada automáticamente ni enlazaba historia, esta rama
sube 3 B el residente Classic y baja 3 B el de Next, libera 6 B de BSS y 3 B
bajo el vector IM2, donde quedan 5 B. El coste total de la función frente a
`bb3dd7f` es +292 B residentes en Classic y +284 B en Next. SP_GAP es la distancia del mapa, no el
máximo dinámico de pila; el aviso Classic `< 768 B` ya existía.

SpectraNext (`make spectranext-size-report SPXN_DIR=C:/dev/SpectraNext/driver`,
overlays de 4.096 B), antes y después de integrarla en esta rama:

| Métrica | Sin recuperación | Con recuperación |
|---|---:|---:|
| Residente | 35.627 B | 35.992 B (+365) |
| SP_GAP (suelo 512, aviso 768) | 1.055 B | 689 B |
| SAVELOAD | 3.918 B | 4.069 B (27 B libres) |
| CONTROL / RECOVERY | 1.126 B / — | 2.997 / 4.091 B |

CONTROL y RECOVERY enlazan cada uno su copia del adaptador XFS. RECOVERY
incluye el replay de la lista y `RETIRE`; le quedan 5 B. SP_GAP baja del umbral
de aviso de 768 B, no del suelo de 512 B; es distancia del mapa, no pila
medida.

## Evidencia

- `tests/tools/test_recovery.py`: ASM productivo (RECOVERY, slot store y
  resume) en Z80 contra un esxDOS multihandle con fallos inyectados:
  alternancia y generación modular, `SRC1` ignorado, flags residentes, escaneo
  incierto sin escritura, sustituciones interrumpidas en cada llamada,
  reglas de oferta, stage completo, parcial y largo (dos bloques), y cada
  degradación a `RESTORED` (CRC, TMP corto o ausente, sin enlace, sin
  registros, errores de lectura, creación, escritura y cierre de NEW).
- `tests/tools/test_journal_link.py`: el append productivo mantiene cuenta y
  CRC en partida nueva, adopción inmediata al aplicar (completa, parcial,
  larga), RESTORE remoto, aplicación sin armar, fallo de sync, fallo de sync
  al adoptar (NEW conservado), fallo de creación y NEW perdido con aviso, y
  adopción diferida.
- `tests/spectrum/test_recovery_capture.c` y
  `tests/tools/test_recovery_capture.py`: el C de captura en host y compilado
  con z88dk en Z80 (codificación in situ, flag reanudable, OVER y ply 0).
- `tests/session/test_direct_session_parity.c`: contrato de aplicación con
  `app.c` real: checkpoint en START, jugadas, TAKEBACK; oferta solo con el
  aviso READY del host (el rol lo filtra `transitions.inc`, no este test),
  espera de Y/N, rechazo sin efectos, aceptación por LOAD, sin candidato,
  Y con rival presente (envía `RQ` y no aplica ni escribe checkpoint) y su
  rechazo `RN`, color incompatible y RESTORE de invitado; la desconexión del
  menú y el `BYE` del rival invocan `RETIRE`, una conexión perdida no.
  `tests/spectrum/test_session_transitions.c`: solo el `BYE` con texto
  (DIRECT y MQTT) retira; el `BYE` lógico vacío de MQTT no.
  `test_recovery_capture` (host y Z80 con el C compilado por z88dk): `RETIRE`
  no escribe fuera de partida viva ni sobre una terminada, y en partida viva
  escribe un único registro no ofrecible con la posición exacta y flags
  ACTIVE|OVER en el propio registro aunque el contexto traiga otro puntero.
- `tests/tools/test_recovery_spectranext.py` con `tests/tools/xfs_model.py`:
  append, RECOVERY y resume de SpectraNext juntos en Z80 contra un modelo del
  adaptador XFS (un handle, errores por `spxn_rom_error()`, RAM frente a
  flash, reemplazo atómico). Partida con START, jugadas y TAKEBACK; corte de
  alimentación que solo conserva lo comprometido; oferta, stage, aplicación y
  jugada siguiente enlazadas; TMP ausente tras el corte degrada a `RESTORED`;
  fallos de escaneo, reemplazo y relectura; temporal comprometido que el
  helper no puede sustituir; reglas de oferta; stage parcial, largo (tres
  bloques) y cada fallo de apertura, lectura o escritura; consumo al aplicar,
  fallo con aviso y TMP ya en flash truncado sin `O_CREAT` ni segundo commit.
  El modelo aplica las reglas de hardware de SpectraNext (EOF con error, EIO
  al borrar un fichero comprometido) y exige rutas y registro en RAM baja. El
  vector de adopción de SpectraNext en `test_spectrum_asm_vectors.py` también
  devuelve EOF con error.
- Replay de la lista: los vectores Classic de `test_spectrum_asm_vectors.py`
  (bloques, UNDO, lecturas cortas, fallos y marcador) se repiten con el NEX
  ensamblado para Z80N (`run_next_journal_replay_vectors`, incluida la
  adopción y la jugada siguiente). `test_recovery_spectranext.py`
  (`check_replay`) ejecuta RECOVERY entrada 2 contra el modelo XFS: reanudación
  completa (sondeo, stage, replay, aplicación, jugada), fronteras de 128 B,
  UNDO en la costura, 240 registros, una apertura por lectura sin handle
  residual, y `RESTORED` ante cuerpo parcial, ausente o impar y fallos de
  apertura, lectura, cierre o segunda pasada.
- `make full-check` verde el 2026-09-28 en una invocación, tras regenerar los
  baselines ABI/tamaño: suite host/ASM, guards, ABI Classic/Next, tamaños sin
  crecimiento, 265 casos BOARD por target e IY. El TAP de Next
  (`next-size-report`) no es un producto: Next usa solo el NEX.
- Hardware 2026-09-28: Classic (esxDOS) y Next (NEX) restauran la partida tras
  cortar la alimentación; en Next, FILE SAVE/LOAD vuelve a funcionar y START ya
  no muestra `Recovery failed`.
- Hardware 2026-09-29, SpectraNext: instalación con SPXI v2, arranque tras
  reinicio y recuperación de la partida correctos; la lista mostraba solo
  `RESTORED` (tres jugadas). El replay de este documento lo corrige; falta
  repetir la prueba en Next y SpectraNext.
- Pendiente de hardware: corte de alimentación real durante append y
  checkpoint, latencia de SD, UART/CTS durante el stage y prueba Classic↔Next
  con DIRECT y MQTT. En SpectraNext, además: `O_WRONLY|O_TRUNC` sobre el TMP
  en flash, latencia de las escrituras en flash por jugada, apagado real y el
  resto de un corte entre commit y `RENAME` (antes de fallar,
  `spxf_replace_atomic()` reabre ese temporal con `O_CREAT`, paso que
  SpectraNext no ha probado; es código de la librería, no de este producto).

## Archivo: plan consolidado del 2026-09-26 (retirado)

> Retirado el 2026-09-28. Los ficheros de su hito 1 (`tools/recovery_tmp_format.py`,
> `asm/esxdos/esx_recovery_open.asm`, `esx_recovery_seek32.asm`, sus vectores,
> `tests/session/test_recovery_barrier_contract.py` y el parser `GC1`–`NE1` de
> `game_protocol`) se eliminaron del árbol; siguen en `2f8414b`.

Esta revisión incorpora la evaluación de las cuatro auditorías externas
(`work/external-audits/game-recovery/registro.md`, archivo privado del
proyecto que no se publica en el repositorio público).
El código todavía implementa el prototipo `.RCV` documentado al final. El
hito 1 dispone de contratos experimentales, vectores de formato/barrera y una
medición enlazada de cota inferior, detallada abajo. Faltan el presupuesto
completo y los hitos 2–5; el prototipo y su gate no validan el diseño nuevo.

El plan de trabajo y las reglas técnicas quedan fijados aquí. El primer hito
cerrará el layout binario y la gramática de sesión con pruebas y medidas;
son entregables técnicos, no nuevas decisiones de producto. Decisión explícita
del usuario, 2026-09-26: **por ahora el invitado recibe solo posición y relojes**.
La transferencia del historial anterior queda fuera de esta fase.

### Criterios de aceptación

- Un único diario activo por equipo: `SHATJRNL.TMP`, fuente de recuperación y
  del historial de SAVE `.STJ`. Eliminar lectores y escritores `.RCV`; no
  asociar temporales antiguos sin un vínculo demostrado.
- Solo el host examina y ofrece su diario, con rival listo y antes de GAME START,
  movimientos o relojes. El TMP del invitado no compite, aunque sea más reciente.
- Rechazar o cancelar una oferta conserva el diario y no inicia otra partida.
  Elegir una nueva partida invalida la anterior antes de permitir jugar, también
  en RESET/rematch. Un reinicio antes del primer MOVE no debe volver a ofrecerla.
  Si falla la invalidación, no declarar preparada la partida: error visible,
  juego bloqueado y salida recuperable.
- Recuperar tablero, turno, ply, enroques, en passant, flags y ambos relojes
  GAME/TURN del último estado completo válido. No sumar tiempo desconectado,
  recuperar MOVE pendientes ni convertir TURN a cero por conveniencia.
- Conservar los colores de la sesión y la perspectiva local del invitado.
  Guardar el lado local del escritor para detectar cambios de asiento entre
  sesiones; se permite cambiar de rol solo si se conservan los lados y el
  snapshot es compatible. No se presume identidad personal del rival.
- Conservar la historia que exista, su parcialidad y su continuidad. Una base
  cargada es recuperable aunque aún no tenga un MOVE posterior. No inventar
  tiempos, snapshots históricos ni profundidad adicional de TAKEBACK.
- Mantener STJ v1, sus validaciones y los relojes actuales del guardado manual.
  Convertir por bloques al guardar/cargar; nunca reescribir toda la historia
  en cada jugada ni cargarla entera en RAM.
- Mantener límites y baselines de residente, BSS, pila y overlays, sin anidación.
  La oferta automática no se sustituye por un comando manual para hacerla caber.
- Reconstruir en Classic y Next las filas que puedan atribuirse al historial
  disponible; conservar la frontera RESTORED donde el origen sea parcial.
  El chat y el cursor quedan fuera del diario.

### Alcance del invitado: solo posición y relojes

| Variante | Resultado | Trabajo específico |
|---|---|---|
| Aprobada: solo posición y relojes | Host conserva su historia; invitado adopta una base parcial y muestra RESTORED antes de las jugadas nuevas | RESTORE transporta la instantánea; hace falta igualmente resolver la barrera de reanudación |
| Diferida: también historial | No se implementa en esta fase | Requeriría una nueva decisión de alcance, transferencia negociada y presupuesto propio |

RESTORE actual lleva 45 bytes binarios codificados en 60 caracteres Base64URL:
no lleva historia. El invitado conserva como parcial su base y las jugadas
posteriores; sus SAVE no declaran recibido el historial anterior. El host
conserva y continúa el cuerpo disponible, incluida su parcialidad de origen.
MACH es informativo, no negociación. Esta reducción de alcance no elimina
la necesidad de confirmar aplicación y habilitación en los dos equipos.

### Transiciones persistentes

El nuevo diseño separa sondeo, preparación, compromiso y continuación. No basta
añadir una escritura dentro de `game_start_state`: hay rutas que ya enviaron
ACK antes de llamarla. Las reglas siguientes se aplican a ambos equipos.

| Transición | Compromiso requerido | Corte, rechazo o fallo |
|---|---|---|
| Sondear / ofrecer | Lectura sin cambiar archivos, `fd/pend`, tablero ni relojes; restaurar posición del handle vivo | Error de E/S significa estado incierto: no ofrecer ni sobrescribir; conservar la salida de error separada del sondeo |
| Nueva partida / RESET / rematch | Preparar el cambio conservando el estado anterior; registrar una intención durable y después su decisión, antes de habilitar juego | Antes del compromiso se puede abortar y conservar la partida anterior; después no se vuelve a ella silenciosamente |
| LOAD local / RESTORE aceptado | Persistir la base aceptada y el cuerpo disponible, validando la adopción antes de habilitar continuación | Nunca esperar al primer MOVE; una transferencia o copia incompleta no se ofrece como partida completa |
| Recuperar el TMP propio | CONTINUE conserva base, cuerpo y marca de parcialidad; abre sin truncar y continúa el prefijo validado | No reutilizar el armado genérico de LOAD ni `pend=0xC0` del prototipo |
| MOVE confirmado / TAKEBACK aceptado | Un solo registro, después de actualizar flags, resultado y TURN | Duplicados solo reenvían respuesta; MOVE pendiente no se persiste |
| SAVE manual | Capturar tablero y relojes actuales, exportar la historia válida y revalidar el STJ | No tomar los relojes de la última jugada como sustituto del estado actual |

**Política de compromiso:** la preparación no destruye el diario anterior.
La intención identifica transición, generación anterior y destino; una
intención sin decisión final bloquea la oferta automática hasta resolverla.
Un aborto confirmado restaura la candidatura anterior; una decisión de cambio
confirmada la invalida aunque todavía no haya jugadas nuevas. Estas marcas
pertenecen al TMP o al staging de esa operación, no a un segundo diario por jugada.

El acuerdo de sesión debe mantener a los dos participantes en preparación hasta
que puedan confirmar sus cambios persistentes. Un fallo no se resuelve dejando
jugar a uno ni con una bandera de «no ofrecer» solo en RAM: se detiene la
transición, se avisa y, si no puede resolverse en el enlace, se desconecta.
Tras reiniciar se examina la decisión durable, sin adivinar por longitud o ply.
El hito 1 debe demostrar estos puntos de corte para START, RESET y rematch,
incluidos CANCEL/NACK y pérdida de los mensajes de compromiso.

El cierre de un handle o un F_SYNC fallidos pueden ocurrir después de que un
truncado haya tenido efecto. No se promete conservar el archivo anterior
después de ejecutar CREATE_AL. Se prepara una salida verificable antes de
cualquier sustitución destructiva.

Un fallo del append durante una partida ya comprometida mantiene el contrato
del diario: no deshace la jugada ni cambia su ACK; deshabilita append, marca la
historia viva parcial y avisa. Una recuperación posterior ofrece explícitamente
el ply del último prefijo persistido, que puede ser anterior al estado perdido.
Esto es distinto de permitir iniciar otra partida tras fallar su invalidación.

### Formato del único TMP

**Primera candidata: base e historia importada, seguidas de eventos con
instantánea.** Reutiliza el codec y evita reconstruir el tablero para recuperar.
Se compara con la variante compacta antes de congelar tamaños; ninguna cifra
de 50/51/70/72 bytes queda aprobada sin incluir todos los campos y límites.

El esquema lógico obligatorio, independiente de la codificación elegida, es:

1. Cabecera versionada con identidad de generación, lado local del escritor,
   integridad y marca de historia completa/parcial.
2. Base restaurable con GAME/TURN y ply, más descripción del cuerpo importado
   si existe: longitud, cuenta e integridad. Un STJ aporta su snapshot **final**
   y su cuerpo MOVE16/UNDO; no aporta snapshots ni relojes de cada jugada antigua.
   Ese cuerpo precede históricamente a la base, no se reproduce desde ella.
3. Eventos posteriores a la base: MOVE/UNDO, orden dentro de la generación y
   validación que vincule evento, snapshot y prefijo anterior. Los controles
   de cambio de partida tienen semántica distinta de UNDO.
4. En una importación o sustitución, identificación de transacción y finalización
   verificable con tamaño/contenido esperados. Un prefijo con CRC correcto no
   equivale a una importación terminada.

El lector devuelve tres resultados distintos: prefijo válido, fin por contenido
incompleto/inválido, o E/S incierta. Se detiene en la primera ruptura; no salta
registros para rescatar snapshots posteriores atribuyéndoles historia anterior.
La integridad debe detectar orden/generación incorrectos y cola antigua, además
de cortes. CRC no demuestra atomicidad de sectores ni integridad absoluta de FAT.

**Continuación elegida:** apertura de actualización sin truncar para un TMP
íntegro, conservando el handle cuando ya esté vivo. Verificar el wrapper y la
semántica esxDOS en el primer hito; la ausencia actual del wrapper no implica
ausencia de la API. Para una cola dañada o una sustitución se prepara un NEW
sellado con el prefijo/base correctos y se adopta como transacción recuperable.
No sobrescribir un registro roto y aceptar luego los bytes antiguos que queden.

`SHATJRNL.NEW` es staging transitorio de reparación/importación/recepción.
Nunca recibe el append normal de cada MOVE. Antes de ofrecer un candidato se
resuelve una adopción pendiente mediante identidad, decisión, tamaño y sello;
un TMP aparentemente válido pero incompleto no prevalece sobre esa transacción.
No se adopta cualquier NEW encontrado ni una carga preparada que el peer rechazó.
Después de verificar el TMP completo se puede retirar el staging. La resolución
de adopción es una fase explícita; el sondeo ordinario sigue siendo solo lectura.

El primer hito fija magic, campos, tamaños, CRC/vinculación, cuenta máxima,
offset máximo y comportamiento al alcanzar el límite: preservar el prefijo,
avisar y deshabilitar append sin wrap ni truncado silencioso. El cuerpo STJ
actual admite como máximo 32767 registros; el seek envuelto usa 16 bits.
Esto no convierte 64 KiB en un límite universal para toda E/S secuencial.
El TMP antiguo de 2 B por registro no contiene base ni relojes: no se ofrece
como recuperable. No borrar automáticamente los RCV antiguos de la tarjeta.

#### Hito 1 — candidato byte a byte bajo prueba

El vector ejecutable `tools/recovery_tmp_format.py` fija por ahora los bytes
siguientes para comparar costes. **No es todavía un lector/escritor esxDOS** y
el layout no se congela hasta el enlace Classic/Next con oferta y barrera. Todos
los enteros son little-endian; los CRC son CCITT-FALSE, inicio `ffff`, sobre
los bytes anteriores indicados. Un snapshot es exactamente el Base64URL v1 de
60 bytes que ya incluye tablero, turno, ply, flags y GAME/TURN.

| TMP base, 86 B | Bytes | Significado |
|---|---:|---|
| Magic / versión | 0..4 | `SJT2`, `02` |
| Flags / operación / reservado | 5..7 | Bit0 historia completa, bit1 lado local negro, bits restantes cero; operación 1 nueva, 2 LOAD, 3 RESTORE, 4 reparación; cero |
| Generación / padre / transacción | 8..19 | Tres `u32`; generación no cero y estrictamente mayor que padre, sin wrap; transacción no cero |
| Historia importada | 20..23 | Cuenta `u16` y CRC `u16` del cuerpo STJ v1 de 2 B por registro |
| Base final | 24..83 | Snapshot v1 completo de 60 B; la historia importada precede a esta posición |
| CRC de cabecera | 84..85 | CRC de 0..83 |

Siguen `2 * imported_count` bytes STJ v1, válidos y contiguos, y un sello
`BEND` de 16 B: magic 0..3, generación 4..7, transacción 8..11, CRC importado
12..13 y CRC 14..15 sobre **cabecera + cuerpo + primeros 14 B del sello**.
La base es recuperable sin MOVE posterior. Un cuerpo completo tiene como
máximo 32767 registros sumando importados y eventos posteriores.

Cada evento posterior ocupa 73 B: tipo 1 B (`1` MOVE, `2` UNDO_ONE), secuencia
`u16` 1..32767, MOVE16/UNDO `u16`, generación `u32`, CRC anterior `u16`,
snapshot final 60 B y CRC `u16` de los primeros 71 B. El primer CRC anterior
es el del sello; los demás apuntan al evento inmediatamente anterior. El ply
del snapshot avanza uno en MOVE y retrocede uno en UNDO. No se salta una
ruptura. Después de una cola rota se repara por NEW con generación distinta;
jamás se reescribe el registro roto dejando una cola antigua atribuible.

`SHATJRNL.NEW` contiene exactamente una base y los eventos que se vayan a
adoptar, seguidos **solo tras el compromiso** de una decisión de 22 B: `DCD1`,
generación `u32`, transacción `u32`, cuenta de eventos `u16`, CRC final de
cadena `u16`, longitud anterior a la decisión `u32` y CRC `u16` de los 20 B
anteriores. Un NEW sin decisión válida bloquea la oferta; no autoriza adopción
ni sobrescritura. Con decisión válida, NEW prevalece sobre un TMP antiguo o
una copia parcial aunque el prefijo TMP valide. Si TMP ya contiene la misma
base/identidad y eventos válidos posteriores, se conserva y se retira NEW
tras verificación. Una E/S incierta bloquea; solo ENOENT confirma ausencia.

La longitud máxima lógica de TMP para 32767 eventos y ningún importado es
`102 + 73*32767 = 2 392 093 B` (NEW suma 22 B). Una importación máxima de
STJ ocupa `102 + 2*32767 = 65 636 B` antes de nuevos eventos; por tanto el
wrapper actual de seek absoluto de 16 bits **no basta**. El lector puede
recorrer secuencialmente y el append necesita handle vivo o una apertura de
actualización sin truncar y posicionamiento de 32 bits verificado. Al llegar
al límite de cuenta o de posicionamiento, se conserva el prefijo, se avisa y
se deshabilita append. No se sustituye esta condición por wrap.
Los wrappers candidatos `asm/esxdos/esx_recovery_open.asm` y
`esx_recovery_seek32.asm` pasan un vector Z80 de apertura de actualización,
handle crudo cero, seek de 32 bits, clipping, error e IY; aún no están enlazados
ni prueban comportamiento físico de esxDOS/SD.

La opción compacta conserva cabecera, cuerpo importado y sello; el evento
requeriría como mínimo 20 B (tipo1, secuencia2, MOVE16 2, generación4, CRC
anterior2, GAME/TURN6, flags1 y CRC2). Necesita replay real de BOARD y rehacer
UNDO inmediato, con alternancia de overlays y servicio de red. Su máximo de
datos sería `102 + 20*32767 = 655 442 B`, antes de código y latencia. La
reducción de E/S por evento es 53 B; no se contabiliza como ahorro total sin
coste enlazado del replay y la barrera.

### Negociación y habilitación del juego

La sesión, no la UI, es dueña de los bloqueos. Se reutilizan las puertas
existentes de CONFIRM/transferencia: los caminos actuales ya bloquean teclado,
editor y cursor durante RESTORE. La oferta nueva necesita además cubrir su
fase previa al RQ, los mensajes entrantes y los relojes.

Secuencia requerida:

1. Handshake nuevo y peer ready; comprobar compatibilidad y candidato del host
   antes de mostrar la oferta. HELLO repetido no abre otra decisión.
2. Host decide; su rechazo conserva el archivo. Si acepta, solicita consentimiento
   al invitado; rechazo, cancelación o caída antes del compromiso conservan el
   candidato del host.
3. Transferir y validar con identificador de transacción ligado a la sesión
   actual. Preparar persistencia; no habilitar juego mientras falten bloques,
   validación o decisión persistente.
4. Separar **estado aplicado**, **confirmación de aplicación del rival** y
   **habilitación local**. El host no aplica por haber enviado los bloques:
   conserva el orden basado en aceptación del peer. El invitado que envía RA
   permanece congelado hasta recibir confirmación de que el host aplicó.
5. Si se pierde RA, repetir de forma idempotente sin aplicar, guardar o registrar
   dos veces. Agotado el plazo, detener el enlace y permitir una oferta nueva
   desde el mismo diario al reconectar. Nunca usar GAME START para completar
   recuperación: reiniciaría los relojes restaurados.

La barrera necesita confirmación de aplicación en ambos sentidos. El hito 1
especifica sus mensajes, correlación, punto de habilitación de cada peer y caso
de pérdida de **cada** ACK, incluido el último. Mientras un equipo no tenga la
confirmación exigida, no habilita movimientos ni relojes. No se promete arranque
simultáneo o conocimiento atómico en dos máquinas: la garantía es local, con
reintentos acotados, bloqueo ante estado incierto y desconexión al agotar plazo.
El transcript debe comprobar también al receptor; el timeout del host por sí
solo no acredita esta barrera.

RESTORE actual basta para transportar la instantánea; no basta como prueba de
esa nueva semántica. Si hacen falta mensajes nuevos incluso sin historial,
serán negociados. Antes de implementar se enmiendan
[wire](wire-contract.md) y [session](session-core-contract.md): recuperación es
una transacción consentida en una sesión nueva, no continuación implícita del
enlace perdido. Un peer sin la capacidad necesaria conserva inicio/carga manual;
la oferta automática incompatible no se anuncia ni se degrada silenciosamente.

No añadir bloques de historial remoto en esta fase. Cualquier mensaje nuevo
necesario para la barrera debe respetar los 47 bytes máximos de payload,
incluyendo verbo, identidad y codificación; STJ v1 no cambia.

### Presupuesto, overlays y servicio de red

| Opción | Coste / riesgo que se debe medir | Decisión |
|---|---|---|
| Base + eventos con instantánea del codec | Más E/S; validar formato, captura, exportador y pila | Primera candidata por reutilización |
| Base + eventos compactos con ambos relojes e integridad | Replay real desde la base; regenera UNDO inmediato, pero exige código y servicio de red acotados | Comparar; no declararlo imposible ni ampliar TAKEBACK |
| Cabeceras alternas en el TMP | Seek, orden de sync y consistencia de zonas | Diferir salvo ventaja demostrada |
| TMP + RCV permanentes o STJ completo por jugada | Dos escritores o trabajo creciente por MOVE | Descartar |

El primer experimento incluye **oferta mínima y barrera de sesión**, además del
lector/escritor/exportador; no se pospone su coste residente hasta acabar formato.
Solo después de comparar ese conjunto se congela el layout.

Classic del prototipo: 35667 B de residente, 17 B físicos antes de IM2 y 599 B
de distancia estática a SP; el techo versionado de residente es 35676 B, por lo
que ese gate deja 9 B, no 17. No se relaja el piso de baseline de 599 B usando
el suelo absoluto de 512. Next: residente 32816 B, SP gap 3369 B y suelo de
proyecto 3072 B. Son medidas existentes del prototipo, no del diseño nuevo.

Classic limita cada overlay a 2048 B; Next NEX, a 4096 B ejecutables dentro de
su página de 8192 B. Actualmente Classic RECOVERY/JOURNAL/SAVELOAD/RESTORE miden
2028/1923/1655/1885 B. Presupuestar sustitución de código, no adición ingenua;
retirar RCV no es ahorro frente a main. Comparar por separado artefactos de
`bb3dd7f`, límites versionados y prototipo, con configuración/hashes acreditados.

#### Medición inicial del hito 1, aún insuficiente para cerrar el layout

Se probó una variante enlazada de **cota inferior**, con la misma configuración
Classic/Next del prototipo y el overlay RCV todavía presente: se inhibió solo
el define residente `NETCHESSZX_RECOVERY` y se añadió un sondeo volátil de
candidato, aviso Y/N y bloqueo local de entrada/reloj. El sondeo presupone el
resultado del lector y no implementa ni el TMP nuevo ni la barrera de red. Tras
medirlo se retiraron esos ganchos: no son una implementación de producto. El
gate fue `make full-check` con `RECOVERY_BUDGET=1`,
`ZX_EXTRA_CFLAGS=-DNETCHESSZX_RECOVERY_BUDGET` y
`BUILD_DIR=build/recovery-budget`; pasó sin cambiar baselines. Sus informes
Classic/Next tienen SHA-256 `6abb7e67840d7cc736d32af3854deb6bb607dad7a7b39ecbac0f6e8f1d4bce6e`
y `95a809980a8b1be473614beb278e3edc627b947ec955e6a729c7e939f7242748`.
El log fue `/tmp/netchesszx-recovery-budget-full-check.log`, SHA-256
`354266c40f790860451d2d77bfd9a849cb9bec57d2ae475066f360b32943a2f3`.

| Enlace | Main anterior, V11 | Prototipo | Cota inferior H1 | Límite |
|---|---:|---:|---:|---:|
| Classic residente | 35387 | 35667 | 35541 | 35676 |
| Classic SP gap | no rehecho | 599 | 599 | 599 |
| Next residente | 32529 | 32816 | 32683 | sin techo residente aquí |
| Next SP gap | 3657 | 3369 | 3501 | 3072 |

La variante enlazada acumula 154 B residentes más que el main anterior en ambos
destinos; ese delta incluye cambios del experimento y no aísla el coste de la
oferta. Classic deja 135 B bajo el techo versionado. No suma como ahorro
retirar RCV: el overlay de la prueba sigue ocupando 2028 B Classic, y main no
lo tenía. Falta enlazar lector, escritor, exportador, sondeo real, transacciones
NEW y **todos** los estados y mensajes de la barrera. Por eso ni los 135 B ni
el gate verde prueban cabida de la variante de 73 B; tampoco prueban que la
compacta de 20 B resulte más barata en código y tiempo. Su ventaja demostrada
es solo 53 B de E/S por evento, a cambio de replay BOARD real y servicio de
red durante el recorrido. Se mantiene la primera candidata por reutilizar el
codec y evitar replay; ambas siguen pendientes de comparación enlazada total.
El próximo presupuesto debe sustituir RCV y contener oferta, barrera y TMP
productivos antes de congelar bytes o avanzar al hito 2.
Los vectores `tests/tools/test_recovery_tmp_format.py` prueban cortes en NEW,
adopción, prefijos, START/RESET/rematch sin MOVE, identidad y E/S incierta;
`tests/session/test_recovery_barrier_contract.py` inyecta pérdidas de RA, RC,
AC y de cada confirmación de nueva partida, observando ambos relojes. Son
oráculos de diseño: aún no ejecutan reducers, FSM ni esxDOS del producto.
`netchess_proto_parse_recovery` valida ya en host los verbos y campos del
contrato, incluidos longitud, hexadecimal estricto y transacción no nula;
ningún reducer ni FSM lo despacha todavía. El primer enlace del parser en
residente Next dejó 2651 B de margen a SP, bajo el piso de 3072 B. Se excluye
su compilación Spectrum mientras no tenga consumidor y se presupuestará junto
con la barrera real; esto no acredita que el conjunto vaya a caber.

| Opción para el siguiente enlace | Evidencia y coste | Riesgo / decisión |
|---|---|---|
| Snapshot por evento, 73 B | Vector de prefijo ejecutable; 2 392 093 B máximos sin importados; codec RESTORE existente | **Hacer el siguiente spike enlazado** con oferta, barrera, TMP y exportador reales; no congelar aún |
| Evento compacto, mínimo 20 B | Ahorra hasta 1 736 651 B de archivo frente al candidato anterior; relojes y flags incluidos | **Mantener en comparación**: replay BOARD, UNDO y servicio de sesión aumentan código/latencia sin medida enlazada |
| Retirar RCV, reutilizar RESTORE | RCV ocupa 2028 B Classic; RESTORE ya transporta 60 caracteres | **Hacer al sustituir**, sin contar 2028 B como ahorro respecto al main ni confundir transporte con barrera |
| Cabeceras alternas o STJ completo por MOVE | Más seek/sync o trabajo creciente | **Diferir / rechazar** hasta evidencia de menor coste total |

BOARD aplica jugadas en overlay; el replay actual de filas no reconstruye tablero.
Alternar overlays desde residente es posible sin anidarlos, pero cuesta código
y E/S. Captura, lectura y replay deben conservar la caché RESTORE: el workspace
de snapshot comparte memoria con ella. No cambiar los 67 B de captura en pila
por ese alias sin preservar los duplicados. Medir pila dinámica, IY/interrupciones
y cada vida de scratch, sin prometer «cero RAM» por razonamiento de fuente.

Lecturas de sondeo, reparación, SAVE y recepción trabajan por bloques acotados.
Drenar UART no sustituye volver al servicio de sesión para atender PING,
cancelación y desconexión. No invocar otros overlays con datos vivos en scratch
sin protección. Medir latencia de append+sync y de recorridos con DIRECT/MQTT,
Classic/Next; los 2 B del diario anterior no acreditan el nuevo coste.

### Orden de implementación y condiciones de cierre

| Hito | Entrega concreta | Condición para avanzar |
|---|---|---|
| 1. Contrato y experimento mínimo | Especificación byte a byte de TMP y transacciones; gramática/capacidades de barrera; comparación de formatos incluyendo oferta mínima | Vectores de prefijo/adopción y transcript de RA perdido; presupuesto enlazado Classic/Next sin relajar límites. Si no cabe, revisar la variante aquí |
| 2. Persistencia y ciclo de vida | Escritor único en punto final; CONTINUE; base al aplicar; invalidación y resolución de NEW | Cortes y errores en cada transición, incluidos START/RESET/rematch y antes del primer MOVE; sin resurrección ni historia falsa |
| 3. STJ y presentación | Export/import STJ v1 por bloques y filas Classic/Next desde historia disponible | SAVE con tiempo transcurrido, LOAD parcial/completo, MOVE/UNDO y segunda recuperación conservan estado y cuerpo correctos |
| 4. Integración automática | Oferta host-only, consentimiento, barrera y compatibilidad; invitado recibe solo posición/relojes y conserva historia parcial | Corpus único para reducers comunes y FSM Spectrum, DIRECT/MQTT; observar ambos equipos, archivos y relojes |
| 5. Retirada y cierre | Eliminar RCV y /recover conservando helpers reutilizados; reconciliar documentación con implementación | Diff completo, gates canónicos y límites; no integrar en main en esta fase |

Las modificaciones de contrato preceden al código afectado; la documentación de
comportamiento implementado se reconcilia al terminar. Esta revisión del plan
no modifica todavía esos contratos ni afirma resultados del experimento.

### Interfaz y verificaciones

Pregunta automática persistente Y/N; la oferta identifica ply y estado guardados.
Color/lado incompatible se informa antes del consentimiento; no reasigna asientos.
Rechazar vuelve a espera de inicio, sin crear partida. Una base sin MOVE o un
estado OVER no se elimina del conjunto de candidatos por una regla inventada;
se presenta su estado real y no se activa el reloj si no corresponde.
Por defecto se ofrece una vez por conexión; reconectar permite repetir.
Los textos se comprueban con el límite de 28 caracteres útiles del aviso.

Verificaciones posteriores, **no ejecutadas para consolidar este documento**:

- Escritor/lector productivo: cortes en cada byte; errores open/read/write/sync/
  close; handle 0 y handle vivo; corrupción, cola antigua válida, cuenta/offset
  al límite, sondeo sin mutaciones y continuidad después de reparar.
- Adopción: NEW preparado pero rechazado, intención sin decisión, aborto fallido,
  TMP con prefijo válido incompleto, corte durante copia y después del sello;
  no promover una transacción distinta ni una carga sin consentimiento.
- Sesión: host blanco/negro, TMP en ninguno/uno/ambos, cambio de rol/lado,
  HELLO/RQ/bloques duplicados o retrasados, START/MOVE prematuros, cancelación,
  desconexión y pérdida de cada ACK en READY/ACTIVE/OVER; comprobar bloqueo
  local y clocks del invitado, no solo el resultado del host.
- SAVE/LOAD: relojes actuales de SAVE, GAME/TURN no cero, base sin MOVE,
  historia completa/parcial/importada, mate/ahogado/jaque, enroque, al paso,
  promoción y TAKEBACK repetido; recuperación → MOVE → SAVE → recuperación.
- Pruebas enfocadas durante implementación; un `make full-check` al cierre del
  bloque que necesite host/ABI/tamaño, reutilizando fases válidas. No encadenar
  abi-check, size-check y full-check redundantes. Aplicar validación Qt si cambia
  su código; persistencia automática Qt y port SpectraNext siguen fuera.
- Hardware: cortes reales, SD y F_SYNC, UART/CTS, servicio PING, pila/IY y Next.
  Un CRC o una relectura no garantizan durabilidad física. Si no se cumplen
  los límites, revisar la variante; no retirar oferta o historial acordado.

## Archivo: prototipo `/recover` (sustituido)

Lo siguiente describe el prototipo `.RCV` de `2f8414b` (`SRC1`, 68 B, sin
enlace con el diario, `/recover` manual). Su código se reescribió en esta rama;
`/recover` ya no existe y los registros `SRC1` se ignoran.

### Uso

Las partidas jugadas con esta rama guardan una instantánea después de cada
movimiento comprometido, TAKEBACK aceptado y posición cargada/restaurada.
Un movimiento local pendiente de ACK no es recuperable todavía. GAME START
no reemplaza la recuperación anterior; lo hace el primer movimiento confirmado.

Después de reconectar, escribe `/recover` antes de comenzar otra partida.
Cualquier jugador puede solicitar la recuperación. Se conserva la comprobación
de colores y la aprobación del rival del flujo de carga existente. Rechazar la
solicitud no modifica el diario activo. Esta versión experimental no muestra
una oferta automática al conectar.

Se restauran la posición y los relojes de la última instantánea válida; el
tiempo transcurrido sin alimentación no se añade. Si la copia más reciente
quedó truncada o corrupta, se utiliza la anterior. Un fallo de guardado avisa
`Recovery failed` una vez por partida y no impide continuar jugando.

La continuación muestra `RESTORED` y declara historial parcial. La recuperación
no reconstruye ni atribuye a la posición un `.TMP` anterior: no existe aún un
identificador persistente que los vincule. El temporal se conserva hasta que
el append de la continuación lo sustituye. Un `.TMP` creado antes de esta rama
no se convierte automáticamente en una partida recuperable.

### Persistencia

Dos archivos privados, fuera del filtro `.STJ` del navegador:

- `/SYS/CONFIG/SHATRCV0.RCV`
- `/SYS/CONFIG/SHATRCV1.RCV`

Cada archivo ocupa exactamente 68 bytes:

| Offset | Tamaño | Contenido |
|---|---:|---|
| 0 | 4 | Magic ASCII `SRC1` |
| 4 | 2 | Generación `uint16` little endian |
| 6 | 60 | Instantánea Base64URL del codec RESTORE existente |
| 66 | 2 | CRC-16/CCITT-FALSE de los primeros 66 bytes, little endian |

La generación avanza con cada guardado y se compara con aritmética modular;
no se ordenan copias por ply, porque TAKEBACK lo reduce. Cada operación vuelve
a leer y validar las copias: no depende de estado conservado en el overlay.
La lectura comprueba magic, CRC y tamaño exacto. La carga de la aplicación
añade la validación semántica del codec y de los colores.

Se escribe exclusivamente en la copia distinta de la última válida. Solo se
informa éxito después de escribir, sincronizar, cerrar y releer el contenido
exacto. Un error de E/S al examinar una copia aborta la escritura antes de
truncar un destino; únicamente ENOENT se interpreta como archivo ausente.
Los registros inválidos por contenido pueden descartarse en favor de la otra
copia. Leer/probar disponibilidad nunca modifica archivos ni el estado del
diario. Este pasa a parcial solo cuando se acepta y aplica la recuperación.

El trabajo de almacenamiento es constante con la duración de la partida:
68 bytes escritos por instantánea y lectura de dos candidatos más la
verificación del destino. No se recopia el historial en cada jugada.

### Decisiones y límites

| Opción | Decisión y motivo |
|---|---|
| Recuperar solo el `.TMP` actual | Insuficiente para relojes e historiales sin posición inicial. |
| Reescribir un `.STJ` completo en cada jugada | Descartado: trabajo creciente con el historial. |
| Dos instantáneas pequeñas con generación y CRC | Aplicado: coste constante y copia anterior conservada durante la sustitución. |
| Overlay RECOVERY | Aloja I/O, metadatos y encoder; comparte preparación con el guardado manual. |
| Reutilizar carga/RESTORE | Conserva validación, consentimiento y reglas de sesión. |
| Oferta automática Y/N al conectar | Diferida para reducir residente; se usa `/recover`. |
| Recuperar el historial íntegro | Diferido: necesita vincular inequívocamente diario e instantánea. |

Classic conserva JOURNAL=17 y añade RECOVERY=18. Next usa RECOVERY=17:
la decimoctava página de overlays ocupa la página libre del bundle existente
de 30 páginas. Los identificadores anteriores no cambian. No se anidan
overlays. La aplicación comparte el codec y protege su caché RESTORE frente a
las capturas automáticas; los duplicados siguen recibiendo la misma respuesta.

### Coste enlazado

Medido el 2026-09-26 frente a los artefactos de `bb3dd7f`:

| Métrica | Antes | Experimento |
|---|---:|---:|
| Classic residente CODE+DATA | 35.387 B | 35.667 B (+280) |
| Classic BSS alto, incluido vector IM2 | 412 B | 413 B |
| Classic margen ordinario antes de IM2 | 298 B | 17 B |
| Classic distancia BSS fijo–SP | 599 B | 599 B |
| Classic TAP | 36.055 B | 36.336 B |
| Classic RECOVERY | — | 2.028 / 2.048 B |
| RESTORE, ambos targets | 1.844 B | 1.885 B |
| Next residente CODE+DATA | 32.529 B | 32.816 B (+287) |
| Next BSS alto | 510 B | 511 B |
| Next distancia BSS–SP | 3.657 B | 3.369 B |
| Next RECOVERY | — | 2.084 / 4.096 B de código |

El NEX sigue ocupando 98.816 B y reserva el mismo bundle de 30 páginas.
El margen Classic es muy estrecho. Los 599 B hasta SP son una medida del mapa,
no del máximo dinámico con el snapshot de 67 B y los frames de C/esxDOS.
Los límites previos de residente, BSS y pila no se han aumentado; los baselines
registran únicamente las nuevas constantes ABI y el coste de TAP/overlays.

### Evidencia

- `tests/tools/test_recovery.py` ejecuta el ASM productivo en el emulador Z80:
  alternancia, wrap de generación, copias truncadas/corruptas, handle cero,
  fallos de apertura/lectura/escritura/sync/cierre/verificación y preservación
  de la copia anterior, del destino de lectura y del diario.
- `tests/session/test_direct_session_parity.c` ejercita la integración con
  instantáneas decodificadas, commits, TAKEBACK, caché RESTORE y rechazo.
- `python3 tests/tools/test_recovery_capture.py` compila el C productivo de
  CAPTURE y del encoder con z88dk y lo ejecuta en Z80; comprueba metadatos,
  conservación de la entrada/caché y propagación de fallos. Es una prueba
  enfocada aparte de `make test`, que conserva su contrato sin llamadas a zcc.
- Cierre `make full-check` superado. Se reanudaron solo las fases pendientes
  tras registrar las constantes ABI nuevas y el crecimiento medido de los
  artefactos; no hubo reparaciones de producto durante el cierre. Incluye
  suite host/ASM, guards, ABI Classic/Next, tamaños, empaquetado y 265 casos
  BOARD por target sobre el overlay enlazado, además de las comprobaciones
  enlazadas del editor y preservación de IY.
- Pendientes de hardware: cortes reales de alimentación, latencia de tarjeta,
  UART/CTS durante el guardado y ejecución Next con el nuevo overlay. CRC y
  doble copia no garantizan integridad ante daños del sistema de archivos o
  pérdida de ambas copias por el dispositivo.
