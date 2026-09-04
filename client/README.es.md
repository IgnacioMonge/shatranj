# Cliente de escritorio Shatranj

[English](README.md) · [Documentación del proyecto](../docs/README.es.md)

Cliente de escritorio Qt para Shatranj 1.2. Windows, macOS y Linux utilizan la
misma implementación y admiten ambos transportes:

- **Direct TCP**: el anfitrión escucha a un invitado; el invitado conecta con
  su dirección y puerto.
- **MQTT**: ambos jugadores entran en una sala mediante un broker; el broker
  transporta los mensajes de sesión y presencia.

Los payloads independientes del transporte y las reglas de topics están
definidos en el [`contrato wire`](../docs/wire-contract.md). Esta guía explica
el uso y la compilación del adaptador Qt; no duplica esa gramática normativa.

## Uso del cliente Qt

1. Selecciona `Direct` o `MQTT` e introduce el endpoint o los datos de sala.
2. Elige `Host` o `Guest`; el anfitrión elige color e inicia la partida.
3. Pulsa una casilla de origen y otra de destino cuando sea tu turno.
4. Usa el cuadro de chat; Enter envía la línea actual.
5. Usa estos comandos para acciones de control:

   ```text
   /draw       ofrecer tablas o revancha
   /resign     abandonar la partida actual
   /takeback   solicitar deshacer el último ply aplicado
   /save [name] guardar localmente la posición actual
   /load [name] cargar una posición y solicitar restore al rival
   ```

   Cualquiera de los dos peers enlazados puede iniciar el intercambio explícito
   de restore; el rival debe aceptarlo antes de enviar o aplicar el snapshot, y
   el color del anfitrión guardado debe coincidir con los asientos actuales. El
   estado retenido de MQTT no sustituye al protocolo de restore. Qt pregunta la
   pieza de promoción; los clientes Spectrum promocionan actualmente a dama.

El cliente recuerda los ajustes de conexión y direcciones Direct recientes,
muestra relojes de turno/partida/movimiento y ofrece un registro RX/TX. Cuando
el rival Direct o MQTT está listo, el contexto inferior termina en
`VS ZX|NXT|MAC|LNX|PC|SPCX`; un cliente antiguo aparece como `VS ?`. Para probarlo
contra un anfitrión Spectrum, consulta
[Prueba Direct TCP con hardware](#probar-direct-tcp-con-hardware).

## Arquitectura

```text
common C portable -> core de escritorio -> UI Widgets común -> aplicación/paquete
```

La capa común posee las reglas de ajedrez, parseo y construcción de protocolo,
gramática MQTT, reducers de sesión y formato wire de las partidas guardadas.
El core de escritorio adapta esos contratos a TCP/MQTT, temporización,
persistencia y helpers Qt. El target Widgets común posee la presentación y se
compila una sola vez para la aplicación y su test de integración; el ejecutable
final contiene el punto de entrada y el empaquetado del sistema. Los límites de
CMake evitan forks por plataforma.

## Compilar y probar

Usa los puntos de entrada del `Makefile` desde la raíz del proyecto:

```sh
make client-test   # configurar, compilar y ejecutar las pruebas Qt
make client        # empaquetado de release de la plataforma actual
make tap           # ZX Classic: SHATRANJ.tap, .OVL y .DAT
make nex           # Spectrum Next: SHATRANJ.nex autocontenido
make full-check    # guards de host, Spectrum, ABI y tamaño
```

`make client-test` es el flujo soportado en Windows, macOS y Linux. En Windows,
`client\build-pc.cmd` es un wrapper equivalente para shells interactivos; los
presets CMake de MSVC mantienen el árbol bajo el directorio `build` de la raíz
y proporcionan las DLL de Qt a CTest. No uses un fallback qmake ni builds
CMake raw/ad-hoc.

En Windows, `make client` mantiene su árbol MSVC incremental en
`build\pc-dist`, separado del árbol `build\pc-build` que usa
`make client-test`. Usa `client\build-msvc.ps1 -CleanBuild` solo cuando sea
realmente necesaria una configuración CMake limpia.

Ambos puntos de entrada de Windows lanzan CMake, MSBuild, el compilador y CTest
con sus manejadores estándar heredados normales. No pongas estas herramientas
detrás de pipes de salida gestionados, bucles de heartbeat ni timeouts de un
wrapper: su progreso nativo debe llegar directamente al terminal o agente que
las invoca. El launcher compartido también desactiva el seguimiento de accesos
de MSBuild porque su tracker inyectado puede dejar suspendidos los procesos del
compilador y linker bajo hosts de agentes que gestionan procesos.

`make client` produce el ejecutable Windows, despliega un bundle macOS o
compila el ejecutable Linux soportado y probado contra el Qt del sistema. El
workflow de GitHub Actions **Build Linux AppImage** empaqueta e inspecciona un
AppImage x86_64 autocontenido, lo sube como artefacto del workflow y lo adjunta
a una release publicada. En macOS también instala el bundle actual en
`/Applications/Shatranj.app`; define
`CLIENT_MAC_APPLICATIONS_DIR` para usar otro directorio Applications.

Los targets Spectrum aceptan estas variables cuando se necesita un build
configurado:

```sh
PORT=5000 MQTT_HOST=broker.example MQTT_PORT=1883 MQTT_CODE=1234 make tap
```

## Probar Direct TCP con hardware

1. Compila el target Classic o Next correspondiente (`make tap` o `make nex`).
2. En un host Classic copia juntos `SHATRANJ.tap`, `SHATRANJ.OVL` y
   `SHATRANJ.DAT`; en Next copia el `SHATRANJ.nex` autocontenido.
3. Inicia el host y anota su dirección LAN y puerto configurado (el Classic
   usa `5000` por defecto).
4. En Qt selecciona `Direct` y `Guest`, introduce dirección/puerto y conecta.
5. Espera a que el host inicie la partida y juega cuando el estado indique tu
   turno; el chat está disponible en la misma ventana.

En Spectrum, la línea de entrada acepta `/draw`, `/resign` y `/takeback`; FILE
ofrece guardar y cargar. Las expectativas de protocolo están en el
[`contrato wire`](../docs/wire-contract.md) y el
[`contrato de sesión`](../docs/session-core-contract.md).
