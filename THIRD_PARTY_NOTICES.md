# Third-party notices

Shatranj's desktop packages for Windows, macOS, and Linux dynamically link Qt 6
libraries distributed under the GNU Lesser General Public License version 3.
Their corresponding source is available from
<https://www.qt.io/download-open-source>. The packages keep Qt as replaceable
shared libraries (DLLs on Windows, frameworks on macOS, shared objects in the
Linux AppImage). See `licenses/Qt-LGPL-3.0.txt` and `licenses/GPL-3.0.txt`.

Shatranj does not include Stockfish. When the user asks for it, the desktop
client downloads an unmodified official Stockfish release, distributed by the
Stockfish developers (<https://stockfishchess.org>) under the GNU General
Public License version 3, and runs it as a separate program. Shatranj is not
affiliated with the Stockfish project.

The Lichess-derived boards and Gioco pieces are distributed under AGPL-3.0-or-
later. The selected piece sets have their own upstream terms: California by
Jerry S. is CC BY-NC-SA 4.0; Kiwen-Suwi by neverRare and TotoY by Kosal Sen are
CC BY 4.0; Merida by Armando Hernandez Marroquin is GPL-2.0-or-later; and
MPChess by Maxime Chupin is GPL-3.0-or-later. Source paths, conversion notes,
and per-set provenance are in `assets/lichess/`. Packages carry the applicable
texts under `licenses/`.

Other credited fonts and artwork retain their upstream terms and attribution;
see the project README and the metadata beside each asset.
