# Manual de Shatranj

Shatranj es un cliente de ajedrez en red. Este programa de escritorio juega
contra otro cliente de escritorio Shatranj o contra un ZX Spectrum, ZX
Spectrum Next o SpectraNext con Shatranj, por la red local (Direct) o por
Internet (MQTT).

## Primeros pasos

1. Acuerda con tu rival cómo conectar: **Direct** en la misma red, o **MQTT**
   por Internet.
2. Uno es el **Host** (anfitrión) y el otro el **Guest** (invitado). El
   anfitrión elige los colores e inicia cada partida.
3. Rellena la primera fila (conexión), elige tu rol en la segunda y pulsa
   **Connect**.
4. Cuando ambos estén enlazados, el anfitrión pulsa **Start Game**.

Abre **Preferencias** (botón del engranaje, o Ctrl+, / Cmd+,) para poner tu
nombre, elegir tablero y piezas y configurar el motor Stockfish. Pulsa **?** o
F1 en cualquier momento para abrir este manual.

## Conectar

### Direct (red local)

- **Host**: elige `Direct` y `Host`. El campo de dirección muestra tu IP local;
  dásela a tu rival. El puerto por defecto es 5000. Pulsa **Connect**: el
  cliente espera al invitado ("LISTENING").
- **Guest**: elige `Direct` y `Guest`, escribe la IP y el puerto del anfitrión
  y pulsa **Connect**. Las direcciones recientes se recuerdan en la lista junto
  al campo.

### MQTT (Internet)

- Los dos jugadores eligen `MQTT`, usan el mismo broker (por defecto
  `broker.hivemq.com`, puerto 1883) y el mismo código de **sala** (`NC` y
  cuatro dígitos hexadecimales, por ejemplo `NC1A2B`).
- Uno entra como **Host** y el otro como **Guest**.

Cuando el rival está listo, la barra inferior termina en `VS ZX`, `VS NXT`,
`VS SPCX`, `VS PC`, `VS MAC` o `VS LNX`, que indica contra qué máquina juegas.
Pulsa **Disconnect** para salir.

### MATCH de Spectranext

Shatranj 1.4 incorpora MATCH en cartuchos Spectranext con firmware 1.2 o
posterior. Empareja dos cartuchos mediante el servicio de su firmware.
El cliente de escritorio sigue usando Direct o MQTT; elige uno de esos
modos en ambos lados para jugar entre escritorio y Spectranext.

## Jugar

- En tu turno, pulsa la pieza y después la casilla de destino. Si las ayudas
  están activadas, se resaltan los destinos legales. También puedes escribir
  una jugada como `e2e4` (o `e7e8q` para promocionar) en el cuadro de mensajes.
- El cuadro de mensajes envía chat a tu rival (hasta 42 caracteres). Enter
  envía la línea.
- Comandos que se escriben en el cuadro de mensajes:

  ```text
  /draw         ofrecer tablas
  /resign       abandonar la partida
  /takeback     pedir deshacer la última jugada
  /save [nombre]  guardar la posición en este ordenador
  /load [nombre]  cargar una posición guardada y pedir al rival que la restaure
  ```

- Los botones bajo el cuadro de mensajes piden deshacer, reiniciar u ofrecer
  tablas.
- Los relojes muestran el tiempo de la partida y el de la jugada en curso.
- El botón de barras muestra las estadísticas de la partida sobre el tablero;
  vuelve a pulsarlo para ocultarlas.
- Al terminar, el anfitrión puede pulsar **Restart Game**. El invitado debe
  aceptar; si lo rechaza, la sesión termina para los dos.

## Guardar y cargar

Las peticiones de deshacer, tablas, reiniciar y cargar dan al rival **60
segundos** para responder Y/N. Si no responde, el solicitante retira la pregunta.
La partida sigue conectada aunque se pierda la respuesta a la retirada. La
pregunta del receptor también caduca automáticamente. Una aceptación cruzada
se completa antes de confirmar la siguiente jugada, manteniendo ambas posiciones
sincronizadas. En LOAD, el plazo cubre la aprobación, antes de transferir la
posición. Un rival antiguo puede necesitar que su operador cierre una pregunta
que no pueda retirar automáticamente.

- `/save nombre` (o el botón de guardar) guarda la posición actual en
  `Documentos/Shatranj/Saves` como archivo `.stj`.
- `/load nombre` (o el botón de cargar) propone esa posición a tu rival. El
  rival debe aceptarla, y el anfitrión debe jugar con el mismo color que en la
  partida guardada.
- Las estadísticas empiezan de nuevo desde la posición cargada.

## Preferencias

Las preferencias se abren en su propia ventana. **Aceptar** aplica los cambios
y **Cancelar** los descarta.

### Tablero

Textura del tablero, juego de piezas y ayudas de jugadas legales. Se pueden
cambiar en cualquier momento, también durante la partida.

### Jugador

Tu nombre, hasta diez caracteres de `A-Z`, `0-9`, `-` y `_`. Tu rival lo ve en
la cabecera de la lista de jugadas. `STOCKFISH` está reservado para el motor.
Esta pestaña se bloquea mientras estás conectado.

### Motor

Instala, actualiza y desinstala Stockfish, elige su nivel y la pausa antes
de cada una de sus jugadas, y muestra u oculta el botón de Stockfish (♞) en la
ventana principal. Esta
pestaña se bloquea mientras estás conectado. Consulta la sección siguiente.

## Jugar con Stockfish

Stockfish es un motor de ajedrez libre y gratuito. Shatranj puede dejar que
juegue tu lado contra cualquier rival, también contra un ZX Spectrum.

### Instalar y desinstalar

- **Instalar / Actualizar a la última** descarga de GitHub la última versión
  oficial para tu ordenador, comprueba su huella SHA-256 y la instala solo para
  tu usuario. No se instala nada en el sistema.
- **Desinstalar** borra el motor descargado.
- **Localizar uno existente** usa un Stockfish que ya tengas. Solo se aceptan
  motores que se identifican como Stockfish.

### Dejar jugar a Stockfish

Pulsa el botón **♞**, junto al engranaje: se vuelve violeta y la barra inferior
muestra `STOCKFISH L5`. Vuelve a pulsarlo para jugar tú. El botón aparece
cuando Stockfish está instalado y se puede ocultar con **Show Stockfish in
the main window** en Preferencias, pestaña Motor.
El **nivel**, de 0 (el más débil) a 20 (el más fuerte), se elige en esa pestaña. El marco del tablero se vuelve entonces violeta y el indicador de turno dice
`STOCKFISH THINKING`. **Connect** no se habilita hasta que el motor está listo.

Stockfish responde casi al instante, así que tu rival no tiene tiempo de pedir
deshacer. **Pause before each move**, en esa misma pestaña, le hace esperar de
0 a 30 segundos (0, el valor por defecto, significa sin pausa) antes de enviar
cada jugada; mientras tanto el indicador de turno sigue diciendo
`STOCKFISH THINKING`. Deshacer, reiniciar, cargar, desconectar o el final de la
partida descartan la jugada en espera, y Stockfish vuelve a pensar sobre la
nueva posición. Si durante la pausa respondes a una petición de tablas o de
deshacer de tu rival, la pausa empieza de nuevo.

Mientras juega Stockfish no puedes mover piezas tú, ni pedir deshacer,
ofrecer tablas o abandonar: esos botones desaparecen y se rechazan
`/takeback`, `/draw` y `/resign`. Sí puedes chatear y responder a las
peticiones de tablas y de deshacer que envíe tu rival.

### Lo que ve tu rival

Shatranj nunca deja que el motor pase por una persona:

- Tu nombre se sustituye por `STOCKFISH` en la lista de jugadas del rival.
- Una vez por sesión, al empezar la primera partida o al restaurar una
  guardada, el chat del rival muestra una línea como
  `STOCKFISH 19 PLAYS WHITE - LEVEL 5`.
- La elección y el nivel no se pueden cambiar durante una sesión.

### Licencia

Stockfish lo distribuyen sus desarrolladores (stockfishchess.org) bajo la GNU
GPL versión 3. Shatranj lo descarga sin modificar de la página oficial de
versiones y lo ejecuta como un programa aparte. Shatranj no está vinculado al
proyecto Stockfish.

## Jugar contra un ZX Spectrum

Un ZX Spectrum, Spectrum Next o SpectraNext con Shatranj se conecta igual que
un cliente de escritorio:

- **Direct**: el Spectrum puede ser anfitrión o invitado. Usa la IP que muestra
  su pantalla de conexión, o dale tu IP local cuando sea el invitado.
- **MQTT**: los dos lados usan el mismo broker y el mismo código de sala.

El Spectrum promociona los peones a dama automáticamente. Los nombres, el chat
y los avisos de Stockfish aparecen en la pantalla del Spectrum igual que en el
escritorio.

## Problemas frecuentes

- **El Firewall de Windows pide permiso al hacer de anfitrión**: Windows lo
  pregunta una vez por programa la primera vez que espera conexiones. Elige
  **Permitir** si rivales de tu red tienen que llegar a ti. Las partidas en el
  mismo ordenador (`127.0.0.1`) funcionan en cualquier caso.
- **CONNECT FAILED**: comprueba la IP y el puerto, que el anfitrión esté
  esperando y que ambos equipos estén en la misma red.
- **HOST BUSY**: el anfitrión ya tiene un invitado.
- **MQTT no enlaza**: comprueba que los dos usan el mismo broker y el mismo
  código de sala, y que uno es anfitrión y el otro invitado.
- **No se pudo descargar Stockfish**: comprueba la conexión a Internet, o usa
  **Localizar uno existente** con un Stockfish que ya tengas.
- El botón **Log** muestra los mensajes intercambiados, útil para informar de
  un problema.
