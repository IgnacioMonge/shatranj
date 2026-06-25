<p align="center">
  <img src="docs/assets/shatranj-logo.png" alt="Shatranj" width="520">
</p>

<p align="center">
  <strong>Shatranj. El primer ajedrez online para un ZX Spectrum 48K real.</strong><br>
  Dos m&aacute;quinas, en cualquier punto del mundo, sobre el mismo tablero.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/versi%C3%B3n-1.0-blue" alt="Versión 1.0">
  <img src="https://img.shields.io/badge/objetivo-ZX%20Spectrum%2048K-d52b1e" alt="Objetivo: ZX Spectrum 48K">
  <img src="https://img.shields.io/badge/toolchain-z88dk%20%2B%20SDCC-555" alt="Toolchain: z88dk + SDCC">
  <img src="https://img.shields.io/badge/transportes-TCP%20directo%20%C2%B7%20MQTT-2ea44f" alt="Transportes: TCP directo y MQTT">
  <img src="https://img.shields.io/badge/cliente%20PC-Qt-41cd52" alt="Cliente PC: Qt">
</p>

<p align="center">
  <a href="README.md">English</a> &middot;
  <a href="CHANGELOG.md">Changelog</a> &middot;
  <a href="client/README.md">Notas del cliente PC</a>
</p>

---

Desde 1982 el ZX Spectrum ha jugado al ajedrez contra su propia ROM, contra una cinta, contra quien se sentaba al lado. Nunca jug&oacute; al ajedrez a trav&eacute;s de una red. **Shatranj es la primera vez que lo hace.**

Dos Spectrum reales, en lados opuestos de internet, compartiendo un tablero sobre divMMC y un enlace ESP-AT. O un Spectrum contra el cliente PC incluido cuando solo hay una m&aacute;quina en la habitaci&oacute;n. El mismo protocolo mueve ambos extremos.

Todo lo que necesita una partida completa est&aacute; aqu&iacute; &mdash; pantallas de configuraci&oacute;n, selecci&oacute;n de bando, entrada de jugadas, relojes, chat, tablas y abandono, restart y reset &mdash; y todo vive dentro de 48K de RAM.

### De un vistazo

|  |  |
| --- | --- |
| **Juego** | Spectrum contra Spectrum, o Spectrum contra PC |
| **Conexi&oacute;n** | TCP directo para pares alcanzables · MQTT a trav&eacute;s de NAT/CGNAT |
| **Tablero** | Tres sets de piezas 16×16 · cinco paletas · ayudas de jugada opcionales |
| **Partida** | Relojes, historial, chat, `/draw`, `/resign` |
| **Hardware** | divMMC/esxDOS · UART ESP-AT · toda la aplicaci&oacute;n dentro de 48K |

### Contenido

- [Galer&iacute;a](#galería)
- [Sets de piezas y temas de tablero](#sets-de-piezas-y-temas-de-tablero)
- [La aplicaci&oacute;n](#la-aplicación)
- [Uso de Shatranj](#uso-de-shatranj)
- [Build Spectrum](#build-spectrum)
- [Build del cliente PC](#build-del-cliente-pc)
- [Modos de red](#modos-de-red)
- [Perfil hardware y toolchain](#perfil-hardware-y-toolchain)
- [Mapa del repositorio](#mapa-del-repositorio)
- [Documentaci&oacute;n](#documentación)
- [Agradecimientos](#agradecimientos)
- [Licencia](#licencia)
- [Autor](#autor)

## Galería

<table>
  <tr>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-01.png" alt="Pantalla principal" width="100%"><br><sub>Pantalla principal</sub></td>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-02.png" alt="Configuracion de partida" width="100%"><br><sub>Configuracion de partida</sub></td>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-03.png" alt="Partida en curso" width="100%"><br><sub>Partida en curso</sub></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-04.png" alt="Setup de red" width="100%"><br><sub>Setup de red</sub></td>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-05.png" alt="Chat y movimientos" width="100%"><br><sub>Chat y movimientos</sub></td>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-06.png" alt="Relojes y estado" width="100%"><br><sub>Relojes y estado</sub></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-12.png" alt="Historial de jugadas" width="100%"><br><sub>Historial de jugadas</sub></td>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-13.png" alt="Entrada local" width="100%"><br><sub>Entrada local</sub></td>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-14.png" alt="Dialogo de partida" width="100%"><br><sub>Dialogo de partida</sub></td>
  </tr>
  <tr>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-15.png" alt="Pantalla de juego" width="100%"><br><sub>Pantalla de juego</sub></td>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-16.png" alt="Cliente PC" width="100%"><br><sub>Cliente PC</sub></td>
    <td align="center" width="33%"><img src="docs/screenshots/shatranj-17.png" alt="Vista final" width="100%"><br><sub>Vista final</sub></td>
  </tr>
</table>

## Sets de piezas y temas de tablero

La build Spectrum incluye tres sets de piezas de 16x16: **BRRY**, **SPCY** y **PIXL**. Game Setup tambien expone cinco paletas de tablero: **Classic**, **Blue**, **Green**, **Cyan** y **Magenta**.

<p align="center">
  <img src="docs/assets/piece-sets.png" alt="Sets de piezas BRRY, SPCY y PIXL" width="720">
</p>

<table>
  <tr>
    <td align="center" width="25%"><img src="docs/screenshots/shatranj-07.png" alt="Tema Classic" width="100%"><br><sub>Classic</sub></td>
    <td align="center" width="25%"><img src="docs/screenshots/shatranj-08.png" alt="Tema Blue" width="100%"><br><sub>Blue</sub></td>
    <td align="center" width="25%"><img src="docs/screenshots/shatranj-09.png" alt="Tema Cyan" width="100%"><br><sub>Cyan</sub></td>
    <td align="center" width="25%"><img src="docs/screenshots/shatranj-10.png" alt="Tema Magenta" width="100%"><br><sub>Magenta</sub></td>
  </tr>
</table>

## La aplicacion

Shatranj puede jugarse en dos emparejamientos:

- **Spectrum-Spectrum**: dos Spectrum conectados a red ejecutan el cliente Spectrum y juegan por TCP directo o MQTT.
- **Spectrum-PC**: un Spectrum juega contra el cliente Qt de escritorio incluido, util para sesiones mixtas, pruebas o cuando solo hay un Spectrum real disponible.

El cliente Spectrum es el objetivo principal: controla tablero, setup, entrada de jugadas, relojes, chat, assets runtime y dispatch de overlays. El cliente PC habla el mismo protocolo y ofrece un endpoint moderno para partidas por TCP directo o MQTT.

Todos los pares hablan el mismo vocabulario de red: setup, host/join, inicio de partida, jugadas, ACK/NACK, chat, ping, reset, tablas y abandono. El protocolo es deliberadamente legible porque depurar hardware ya es bastante dificil.

## Uso de Shatranj

### Menus del Spectrum

El lado Spectrum se maneja desde las pantallas de setup antes de entrar al tablero.

- **Connection Setup** elige el transporte y el endpoint: TCP directo o MQTT, Host o Guest, host/broker, puerto y codigo de sala.
- **Game Setup** elige politica de color/bando, notacion, tema de tablero, set de piezas y ayudas.
- **Tab** cambia entre campos editables y opciones. Los cursores cambian la opcion enfocada; los campos de texto usan la linea de entrada normal del Spectrum.
- El **Host** inicia la partida cuando el peer esta enlazado. El Guest espera `GAME START` y lo confirma.

### Durante la partida

- Escribe una jugada de coordenadas como `e2e4` y enviala cuando sea tu turno.
- Escribe cualquier otro texto para enviarlo como chat.
- El chat es corto a proposito: ambos clientes respetan el mismo limite de dos lineas visibles del Spectrum.
- `/draw` ofrece tablas.
- `/resign` abandona la partida.
- Reset/restart requieren confirmacion del rival; la UI mantiene la sesion visible en vez de saltar de estado sin explicacion.

### Cliente PC

- Selecciona Direct o MQTT para coincidir con el setup del Spectrum.
- En Direct, conecta a la IP y puerto del Spectrum host.
- En MQTT, usa el mismo broker, puerto, sala y reparto Host/Guest.
- Puedes jugar en el tablero haciendo click en origen y destino; la caja de chat tambien acepta `/draw` y `/resign`.
- El log RX/TX ayuda con hardware real porque el protocolo de red es deliberadamente legible.

## Build Spectrum

```sh
make tap PORT=5000
```

Copia juntos los tres ficheros de release:

```text
release/SHATRANJ.tap
release/SHATRANJ.OVL
release/SHATRANJ.DAT
```

`SHATRANJ.tap` no basta por si solo. El OVL contiene rutas frias de codigo y el DAT contiene assets runtime. Si el DAT falta, esta corrupto o pertenece a otra build, el Spectrum se detiene al arrancar con borde rojo y `DAT?`. Es intencional: mejor fallar de forma visible que ejecutar con assets desincronizados.

## Build del cliente PC

```sh
make client
```

En Windows, el ejecutable queda empaquetado aqui:

```text
release/shatranj-client/shatranj-client.exe
```

El Makefile elige el mejor backend disponible. Windows usa el script MSVC/Qt. macOS y Linux usan CMake si esta disponible y caen a qmake si hace falta.

## Modos de red

### TCP directo

El modo Direct es para un rival alcanzable en red local o mediante puerto redirigido.

```text
HELLO DIRECT HOST|GUEST
GAME START WHITE=HOST|GUEST
MOVE <ply> <move>
ACK <ply>
NACK <ply>
CHAT <text>
PING / ACK PING
```

### MQTT

MQTT es para jugar mediante broker, util cuando los pares estan detras de NAT o CGNAT.

```text
H W|B <sid>
J <sid>
GAME START
MOVE / ACK / NACK / CHAT
PING / ACK PING
```

El PUBACK de MQTT no cuenta como acknowledgement de partida. Shatranj usa ACK/NACK de aplicacion para que ambos pares acuerden el estado de juego, no solo la entrega de paquetes.

## Perfil hardware y toolchain

- Objetivo ZX Spectrum 48K.
- z88dk `zcc` + SDCC, `sdcc_iy`, `--opt-code-size`, `--fomit-frame-pointer`.
- ROM IM1; el ASM manual trata IY como reservado para el contrato de la ROM.
- Carga de overlays con esxDOS/divMMC.
- UART compatible divMMC/ZX-Uno para red ESP-AT.
- Regiones fijas de low RAM para historial de jugadas, chat, relojes, tablero, contexto de overlay y ayudas.
- Guardas de build para ABI SDCC/IY, solapes de low RAM, layering, ABI de entradas overlay, tamano de overlays, residente y margen de pila.

## Mapa del repositorio

| Ruta | Proposito |
| --- | --- |
| `src/spectrum/` | Aplicacion Spectrum, UI, transporte, sesion, setup y overlays |
| `asm/` | Z80 manual: render, UART, loader esxDOS, entradas de overlay |
| `src/common/` | Ajedrez, protocolo, MQTT y helpers compartidos |
| `src/pc/`, `client/` | Cliente Qt y wrappers de build |
| `assets/` | Assets runtime Spectrum, sets de piezas y datos de About |
| `tests/` | Tests host de protocolo, sesion, ajedrez y limites Spectrum |
| `tools/` | Generacion de assets/overlays, reportes de size, guardas ABI y layering |

## Documentacion

- [CHANGELOG.md](CHANGELOG.md) - notas de version 1.0.
- [README.md](README.md) - README en ingles.
- [client/README.md](client/README.md) - build del cliente PC y pruebas con hardware.
- [docs/source-layout.md](docs/source-layout.md) - arbol de fuentes y limites de propiedad.
- [docs/architecture-decisions.md](docs/architecture-decisions.md) - decisiones de arquitectura duraderas.
- [docs/mqtt-session-policy.md](docs/mqtt-session-policy.md) - politica de sesiones MQTT.

## Agradecimientos

- **Piezas BRRY**: basadas en [Chess Pieces 16x16 One-bit](https://berryarray.itch.io/chess-pieces-16x16-one-bit) de [BerryArray](https://berryarray.itch.io).
- **Piezas SPCY**: basadas en [Chess Pieces](https://spicygame.itch.io/chess-pieces) de [Spicy Game](https://spicygame.itch.io).
- **Piezas PIXL**: basadas en [Pixel Art Chess Pieces](https://benrosen.github.io/posts/pixel-art-chess-pieces/) de [Ben Rosen](https://benrosen.github.io).
- **Fuente Ikkle**: [Ikkle 4](https://www.dafont.com/es/ikkle-4.font) de Brixdee, usada como base para el texto compacto de la UI Spectrum.
- **mcu-max**: motor de ajedrez para sistemas de pocos recursos, con licencia MIT, de [Gissio](https://github.com/Gissio); se conserva en `third_party/mcu-max` con su licencia upstream.

## Licencia

Shatranj es software libre publicado bajo GNU General Public License v2.0.

El codigo y los assets de terceros conservan sus licencias y creditos upstream; ver Agradecimientos.

## Autor

M. Ignacio Monge Garcia - 2026

Conectando el ZX Spectrum al ajedrez online desde 2026.
