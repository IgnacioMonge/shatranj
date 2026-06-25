# Shatranj

Qt desktop app for Shatranj Direct TCP and MQTT sessions.

Current scope:

- Enter Direct TCP host/port or MQTT broker/room/port.
- Remember the last connection settings.
- Connect/disconnect.
- Choose Host/Guest role. The host selects color, starts games, and resets
  running games.
- Click a source and target square to build a coordinate move when it is your
  turn.
- Send `MOVE <ply> <move>` and apply it locally after the opponent ACK.
- Send and receive `CHAT <text>` messages in the chat panel.
- Show current turn feedback plus game and move clocks.
- Send chat with Enter from the message box.
- Show RX/TX log.
- MQTT room setup and peer presence are implemented. Retained state restore is
  not implemented yet.

Local chess legality now runs in the Qt client through the Shatranj rules
wrapper around `mcu-max`.

## Build

Use the repository `Makefile` as the stable entry point:

```sh
make client
```

Backend selection is automatic:

- Windows: MSVC/Qt deploy script, output `release\shatranj-client\shatranj-client.exe`.
- macOS/Linux: CMake if available, otherwise qmake.

To force a backend:

```sh
make CLIENT_BUILD=cmake client
make CLIENT_BUILD=qmake client
make CLIENT_BUILD=msvc client
```

On macOS, CMake/qmake builds also update a `Shatranj.app` entry in
`/Applications`. To use a different Applications directory:

```sh
make CLIENT_MAC_APPLICATIONS_DIR=/path/to/Applications client
```

## Test Direct TCP With Hardware Host

1. Build/copy `release/SHATRANJ.tap`, `release/SHATRANJ.OVL`, and
   `release/SHATRANJ.DAT` on the hardware host.
   A red border with `DAT?` at boot means the DAT file is missing, corrupt, or
   does not match the generated asset size.
2. Read the host IP from `+CIFSR:STAIP,"..."`.
3. Start Shatranj.
4. Select `Direct` and `Guest`.
5. Enter that IP and port `5000`.
6. Connect.
7. Wait for the host side to start the game.
8. After the client receives a `MOVE`, play a move when the status shows it is
   your turn.
9. Use the Chat box to exchange messages with the opponent.

Expected reply:

```text
ACK 2
```

The hardware input line also accepts text: exact coordinate moves are played
during the local turn, otherwise the text is sent as chat. Editing supports
left/right, history with up/down, CAPS+O/P word movement, CAPS+9 word delete,
CAPS+1/2 start/end, and held-key repeat for typing/navigation/backspace.
