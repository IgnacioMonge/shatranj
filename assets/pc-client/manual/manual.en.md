# Shatranj manual

Shatranj is a network chess client. This desktop program plays against
another Shatranj desktop client or against a ZX Spectrum, ZX Spectrum Next or
SpectraNext running Shatranj, over your local network (Direct) or over the
Internet (MQTT).

## Getting started

1. Agree with your opponent on how to connect: **Direct** on the same network,
   or **MQTT** through the Internet.
2. One of you is the **Host** and the other is the **Guest**. The host chooses
   the colours and starts each game.
3. Fill in the first row (connection), choose your role in the second row and
   press **Connect**.
4. When both sides are linked, the host presses **Start Game**.

Open **Preferences** (gear button, or Ctrl+, / Cmd+,) to set your name, the
board and pieces, and the Stockfish engine. Press **?** or F1 at any time to
open this manual.

## Connecting

### Direct (local network)

- **Host**: select `Direct` and `Host`. The address field shows your local IP;
  give it to your opponent. The default port is 5000. Press **Connect**: the
  client waits for the guest ("LISTENING").
- **Guest**: select `Direct` and `Guest`, type the host's IP and port and press
  **Connect**. Recent addresses are remembered in the list next to the field.

### MQTT (Internet)

- Both players select `MQTT`, use the same broker (default
  `broker.hivemq.com`, port 1883) and the same **room** code (`NC` followed by
  four hexadecimal digits, for example `NC1A2B`).
- One player joins as **Host** and the other as **Guest**.

When the opponent is ready, the bottom bar ends with `VS ZX`, `VS NXT`,
`VS SPCX`, `VS PC`, `VS MAC` or `VS LNX`, showing which machine you play
against. Press **Disconnect** to leave.

### Spectranext MATCH

Shatranj 1.4 adds MATCH on Spectranext cartridges with firmware 1.2 or later.
It pairs two cartridges through their firmware service. The desktop client
continues to use Direct or MQTT; choose one of those modes on both sides
when playing desktop-to-Spectranext.

## Playing

- On your turn, click the piece and then the target square. Legal targets are
  highlighted when hints are enabled. You can also type a move such as `e2e4`
  (or `e7e8q` for a promotion) in the message box.
- The message box sends chat to your opponent (up to 42 characters). Enter
  sends the line.
- Commands typed in the message box:

  ```text
  /draw         offer a draw
  /resign       resign the current game
  /takeback     ask to undo the last move
  /save [name]  save the position on this computer
  /load [name]  load a saved position and ask the opponent to restore it
  ```

- The buttons under the message box ask for a takeback, a reset or a draw.
- The clocks show the game time and the time of the current move.
- The bar-chart button shows the game statistics over the board; press it
  again to hide them.
- After the game, the host may press **Restart Game**. The guest must accept;
  declining ends the session for both players.

## Saving and loading

Takeback, Draw, Reset and Load requests give the opponent **60 seconds** to
answer Y/N. If nobody answers, the requester withdraws the question.
The current game stays connected even if the withdrawal reply is lost. The
remote question also expires automatically. A crossed acceptance is completed
before the next move commits, keeping both positions synchronized. Load's
deadline covers approval, before the position is transferred. Older peers may
need their operator to close a question that they cannot withdraw automatically.

- `/save name` (or the save button) stores the current position in
  `Documents/Shatranj/Saves` as a `.stj` file.
- `/load name` (or the load button) proposes that position to your opponent.
  The opponent must accept, and the host must play the same colour as in the
  saved game.
- Statistics restart from the loaded position.

## Preferences

Preferences open in their own window. **OK** applies the changes and
**Cancel** discards them.

### Board

Chessboard texture, piece set and legal-move hints. These can be changed at
any time, even during a game.

### Player

Your name, up to ten characters from `A-Z`, `0-9`, `-` and `_`. Your opponent
sees it in the header of the move list. `STOCKFISH` is reserved for the
engine. This tab is locked while you are connected.

### Engine

Installs, updates and removes Stockfish, sets its level and the pause before
each of its moves, and shows or hides the Stockfish button (♞) in the main
window. This tab is locked while you are connected. See the next section.

## Playing with Stockfish

Stockfish is a free, open-source chess engine. Shatranj can let it play your
side against any opponent, including a ZX Spectrum.

### Installing and removing

- **Install / Update to latest** downloads the latest official release for
  your computer from GitHub, checks its SHA-256 fingerprint and installs it
  for your user only. Nothing is installed system-wide.
- **Uninstall** deletes the downloaded engine.
- **Locate existing** uses a Stockfish you already have. Only engines that
  identify themselves as Stockfish are accepted.

### Letting Stockfish play

Press the **♞** button next to the gear: it turns violet and the bottom bar
shows `STOCKFISH L5`. Press it again to play yourself. The button appears
once Stockfish is installed and can be hidden with **Show Stockfish in the
main window** in Preferences, Engine tab. The
**level**, from 0 (weakest) to 20 (strongest), is set in that tab. The board frame then turns violet and the turn indicator reads `STOCKFISH THINKING`.
**Connect** stays disabled until the engine is ready.

Stockfish answers almost at once, which leaves your opponent no time to ask
for a takeback. **Pause before each move**, in the same tab, makes it wait
from 0 to 30 seconds (0, the default, means no pause) before each move is
sent; the turn indicator keeps reading `STOCKFISH THINKING` meanwhile. A
takeback, a restart, a load, a disconnection or the end of the game discards
the waiting move, and Stockfish thinks again about the new position. An
opponent's draw or takeback request answered during the pause starts it
again.

While Stockfish plays you cannot move pieces yourself, ask for a takeback,
offer a draw or resign: those buttons leave the window and `/takeback`,
`/draw` and `/resign` are refused. You can still chat and answer the draw
and takeback requests your opponent sends.

### What your opponent sees

Shatranj never lets the engine pass for a person:

- Your name is replaced by `STOCKFISH` in the opponent's move list.
- Once per session, when the first game starts or a saved game is restored,
  the opponent's chat shows a line such as
  `STOCKFISH 19 PLAYS WHITE - LEVEL 5`.
- The choice and the level cannot be changed during a session.

### Licence

Stockfish is distributed under the GNU GPL version 3 by the Stockfish
developers (stockfishchess.org). Shatranj downloads it unmodified from the
official releases page and runs it as a separate program. Shatranj is not
affiliated with the Stockfish project.

## Playing against a ZX Spectrum

A ZX Spectrum, Spectrum Next or SpectraNext running Shatranj connects in the
same way as a desktop client:

- **Direct**: the Spectrum can be host or guest. Use the IP shown on its
  connection screen, or give it your local IP when it is the guest.
- **MQTT**: both sides use the same broker and room code.

The Spectrum promotes pawns to a queen automatically. Names, chat and
announcements from Stockfish appear on the Spectrum screen as on the desktop.

## Troubleshooting

- **Windows Firewall asks for permission when you host**: Windows asks once
  per program the first time it listens for connections. Choose **Allow** if
  opponents on your network must reach you. Games on the same computer
  (`127.0.0.1`) work either way.
- **CONNECT FAILED**: check the IP and port, that the host is listening, and
  that both machines are on the same network.
- **HOST BUSY**: the host already has a guest.
- **MQTT does not link**: check that both players use the same broker and the
  same room code, and that one is host and the other guest.
- **Stockfish could not be downloaded**: check your Internet connection, or
  use **Locate existing** with a Stockfish you already have.
- The **Log** button shows the raw messages exchanged, which helps when
  reporting a problem.
