# Wire Contract

This is the client-neutral NetChessZX wire contract. Spectrum, PC, and future clients must implement these payloads without assuming the peer implementation.

## Ownership

`docs/session-core-contract.md` is authoritative: PC executes the canonical
common reducers; ZX and Next execute the compact Spectrum FSMs. These are the
only two implementation families and must produce the same target-neutral wire
semantics.

## Session

| Transport | Direction | Payload | Notes |
| --- | --- | --- | --- |
| Direct | host to guest | `HELLO DIRECT HOST WHITE=HOST|GUEST` | Host announces which role owns white. |
| Direct | guest to host | `HELLO DIRECT GUEST` | Guest readiness. |
| Direct | occupied host to new guest | `BUSY` | Positive occupancy signal; send before closing only the newcomer connection. |
| Direct | host to guest | `GAME START WHITE=HOST|GUEST` | Direct start carries white owner. |
| MQTT | host bootstrap | `H W|B <sid>` | Retained on the side-neutral `meta` topic. |
| MQTT | guest bootstrap | `J <sid>` | Live on the side-neutral `meta` topic. |
| MQTT | side presence | `O W|B <sid>` | Retained on `pres_w` or `pres_b`. |
| MQTT | host to guest | `GAME START` | MQTT color/session comes from presence, not start detail. Receivers may accept `GAME START <detail>` for forward compatibility, but senders should emit plain `GAME START`. |
| Any | receiver to starter | `ACK GAME START` | Start accepted. |
| Any | receiver to starter | `NACK GAME START [reason]` | Start rejected. |

## Game

| Payload | Meaning |
| --- | --- |
| `MOVE <ply> <move> [notation]` | Coordinate move. Promotion suffix may be `q`, `r`, `b`, or `n`. |
| `ACK <ply> [notation]` | Move or takeback accepted. |
| `NACK <ply> [reason]` | Move or takeback rejected. Reason is optional and advisory; `BUSY` means the receiver has a pending decision or correlated request. |
| `TAKEBACK <ply>` | Request undo of the last applied ply. Response is generic `ACK/NACK <ply>`. |
| `CANCEL TAKEBACK <ply>` | Withdraw that pending undo. Reply `NACK <ply>`, or replay `ACK <ply>` if already applied. |
| `DRAW` | Offer a draw; acceptance ends the current game. |
| `ACK DRAW` / `NACK DRAW` | Draw response. |
| `CANCEL DRAW` | Cancel a still-pending DRAW request. The receiver answers `NACK DRAW`. |
| `RESET` | Reset/rematch request. |
| `ACK RESET` / `NACK RESET [reason]` | Reset response. |
| `CANCEL RESET` | Cancel a still-pending RESET request. The receiver answers `NACK RESET`. |
| `RESIGN` | Unilateral resignation. Sender retransmits until `ACK RESIGN` arrives. |
| `ACK RESIGN` | Resignation acknowledged. Receivers must ACK every `RESIGN`, including retransmissions. The finished game remains available until a separately requested RESET is accepted. |
| `CHAT <text>` | Chat text. Max visible text is 42 chars. |
| `MACH` + code (`ZX` / `NXT` / `MAC` / `LNX` / `PC` / `SPCX`) | Optional peer machine identity. Both peers may emit after the link is ready (`peer_ready`). Informational only: never changes game state, color, or control flow. Unknown codes and missing announcements leave the peer platform unknown. Legacy 1.1 peers ignore `MACH` silently on Direct and on Spectrum MQTT (first letter `M` is not `MQTT_TEXT`). |
| `MNAME <name>` | Optional player name. Both peers may emit it after the link is ready (`peer_ready`), next to `MACH` (the Qt client sends it just before `MACH`, so a Spectrum already knows the name when `MACH` repaints its status bar), only when the local player configured a name. `<name>` is 1 to 10 characters from `A`-`Z`, `0`-`9`, `-` and `_`, with no spaces and nothing after it. Informational only: never changes game state, color, or control flow. A receiver ignores a malformed name and keeps showing its color label (`WHITE`/`BLACK`). Legacy peers ignore `MNAME` silently for the same reason as `MACH`. The Qt client reserves `STOCKFISH` for its engine autopilot: while an engine plays its side it always sends `MNAME STOCKFISH` (never the user name), cannot switch player during a session, and sends `CHAT <ENGINE> PLAYS <WHITE|BLACK> - LEVEL <n>` once per session, at the first game start or applied restore. The Spectrum appends `: STOCKFISH` to the peer platform in its status bar (for example `VS MAC: STOCKFISH` from the macOS client) when the peer name is exactly `STOCKFISH` at that repaint. |
| `PING` / `ACK PING` | Keepalive. |
| `BYE` | Peer disconnect. |
| `RQ` | Request permission to restore a saved position. |
| `RY` / `RN` | Accept or reject/cancel a restore exchange. |
| `RS00 <30 Base64URL chars>` | First 30 ASCII characters of the unpadded Base64URL restore encoding. Exactly 35 wire bytes including the `RS00 ` prefix. |
| `RS01 <30 Base64URL chars>` | Final 30 ASCII characters of the unpadded Base64URL restore encoding. Exactly 35 wire bytes including the `RS01 ` prefix. |
| `RA` | Snapshot applied; restore exchange complete. |

## Topic Rules

Direct carries these payloads as newline-delimited TCP lines. A line ends at
LF; a Direct receiver ignores CR (`0x0D`) wherever it appears, so CRLF and LF
framing are equivalent, a CR never reaches a payload, and the payload limit
counts only the remaining bytes. Senders terminate lines with LF and never
put CR inside a payload.

Application payload bytes never contain `0x00`. A locally appended C
terminator is not part of the wire payload. A receiver must reject a Direct
line or MQTT PUBLISH containing an embedded NUL instead of dispatching the
prefix as a shorter message.

The interoperable application-payload limit is 47 wire bytes. Spectrum's
48-byte receive buffers include the local NUL terminator; `CHAT ` plus the
maximum 42 visible chat characters occupies exactly all 47 wire bytes.

MQTT uses room topics by side. Lateral topics name the direction (`w2b` =
white publishes, black listens; `b2w` the reverse). ACK topics name the
recipient side (`ack_w` carries acknowledgements addressed to white, `ack_b`
to black); the sender publishes to the peer's ACK topic.

Normative per-payload topic table:

| Payload | Sender publishes to | Receivers must accept from |
| --- | --- | --- |
| `H` | `meta` (retained) | `meta` |
| `J` | `meta` (live) | `meta` |
| `O` / `F` | own `pres_w` / `pres_b` | peer and own presence topics |
| `GAME START` | `meta` or own lateral (both canonical, see below) | `meta` and inbound lateral |
| `ACK GAME START` / `NACK GAME START` | `meta` or own lateral (both canonical, see below) | `meta` and inbound lateral |
| `CANCEL RESET` / `CANCEL DRAW` / `CANCEL TAKEBACK <ply>` | own lateral, live, never retained | inbound lateral |
| Game payloads (`MOVE`, `TAKEBACK`, `DRAW`, `RESET`, `RESIGN`, `CHAT`, `PING`, `BYE`, `MACH`, `MNAME`) | own lateral (`w2b` white, `b2w` black) | inbound lateral |
| `ACK` / `NACK` responses to game payloads (incl. `ACK PING`) | peer ACK topic (`ack_b` from white, `ack_w` from black) | inbound ACK topic and inbound lateral |
| `RQ`, `RY`, `RN`, `RS00`, `RS01`, `RA` | own lateral, live, never retained | inbound lateral |

`GAME START` and its ACK/NACK are the only payloads with two canonical
emission topics: the Spectrum family publishes them on `meta`, the PC family
on its own lateral. Both are contract-legal until unified; every client must
therefore listen for them on `meta` and on its inbound lateral. Receivers
must process these START replies by payload, not by client type. Receivers
enforce the inbound lateral route for `MOVE`: a `MOVE` received on any other
topic is inert and does not credit liveness, mutate state, reach the UI, or
produce a reply. Other payload families retain their current payload-dispatched
behavior; see `docs/session-core-contract.md`.

An MQTT host installs a retained CONNECT Last Will on its own `pres_w` or
`pres_b` topic with payload `F <side> <sid>`. A guest CONNECT has no Will.
Receivers end a session only for a live peer-side `F` carrying the current SID;
an id-less payload, foreign SID, or retained replay is inert.

## CANCEL RESET / CANCEL DRAW

`CANCEL RESET` and `CANCEL DRAW` cancel a matching remote RESET or DRAW decision
that is still pending. Direct carries CANCEL as a normal line. MQTT carries it
on the sender's own lateral topic, live and never retained.

The receiver answers `NACK RESET` or `NACK DRAW` through the normative
general NACK route while the request is still pending. If it already accepted
the request and no later MOVE invalidated that result, it replays the matching
ACK instead, so a lost acceptance cannot be mistaken for withdrawal. If the
matching decision is pending, it is invalidated, the
NACK confirms cancellation, and a later modal decision is inert. If the request
was declined, does not match, or the CANCEL repeats a confirmed withdrawal,
the receiver sends the same NACK without mutating state. The cancelling sender interprets
that NACK as `CANCELLED`, not as rejection of the original request.

Both the requester and the remote decision use a 60-second deadline; retries
never extend either window. Cancellation keeps its correlation and retries in
the background, without ending a live session. A next MOVE can proceed and its
commit invalidates the old withdrawal. A crossed acceptance still completes the
original control before that commit. The remote prompt also expires without
receiving CANCEL, rejecting the request and leaving the game available.
Older peers may ignore CANCEL and lack the remote deadline; their operator must
close the old prompt. Actual link loss retains its normal failure policy.

## MQTT RESTORE

Either linked peer may initiate MQTT RESTORE. The ordered exchange is `RQ`,
then `RY` or `RN`; after `RY`, the initiator sends `RS00` followed by `RS01`.
The receiver decodes and applies only after both 30-character chunks form the
complete 60-character, unpadded Base64URL encoding of the 45-byte binary save
record.
No padding or NUL terminator is transmitted. It then answers `RA` on success
or `RN` on rejection/failure.

The control deadline gives each stage bounded retries. While waiting for `RY`,
the initiator retries `RQ`; while waiting for `RA`, it retries the two chunks in
order. The receiver re-sends `RY` for a duplicate accepted `RQ`, waits a bounded
time for missing chunks, and ends the exchange with `RN` on expiry. Before
`RY`, a local cancel sends `RN`; after chunk transmission starts, local cancel
is ignored because the peer may already have applied the snapshot. After a
successful apply, either exact duplicate chunk re-sends `RA`; a conflicting
chunk sends `RN` without applying again.

## Game recovery

Recovery adds no payload. After peer ready and before any `GAME START`, a
host holding an offerable local checkpoint asks its own user. On acceptance it
runs the ordinary host-initiated RESTORE exchange above with the checkpoint
snapshot: `RQ`, the guest's normal RESTORE prompt as its consent, `RY`,
`RS00`/`RS01`, then `RA` or `RN`. The snapshot carries position, flags, ply
and both GAME/TURN clocks; history never travels (the host keeps its own, the
guest starts a partial one). A refusal, cancel or failure leaves the game
unstarted; the host's journal TMP and checkpoints are unchanged (only its
unarmed `SHATJRNL.NEW` staging is replaced, as a FILE LOAD would). Older peers
interoperate because nothing new is sent.

The milestone-1 extension drafted on 2026-09-26 (`GC1` to `NE1`) was
withdrawn unimplemented; see `docs/game-recovery-experiment.md`.

## Compatibility Rules

- Parse by payload grammar, not by peer client name.
- `TAKEBACK`, `DRAW`, `RESET` (including a new game after OVER) and RESTORE approval (`RQ`/`RY`) share a 60-second decision deadline, measured from the first successful request transmission. Transport retries do not restart that deadline. PC uses milliseconds; Spectrum uses elapsed hardware frames at the active 50/60 Hz rate, including busy iterations and counter wrap.
- On expiry the requester withdraws the decision: `CANCEL TAKEBACK <ply>`, `CANCEL DRAW`, `CANCEL RESET`, or `RN` before `RY`. The receiver closes its pending prompt and rejects without changing the board; the game/session remains current. TAKEBACK cancellation is correlated to its nonzero ply. A duplicate cancellation is harmless. If a TAKEBACK was already applied when cancellation crossed its ACK, the receiver replays the ACK and the requester completes that same undo rather than leaving different boards.
- The current cancellation exchange requires confirmation where it can race with a board mutation. Exhausted cancellation retries still end the session, including between updated peers; this is an unresolved limitation, not the requested final timeout policy (`docs/handoff.md` §14). Older peers may ignore `CANCEL TAKEBACK <ply>`. Lost links and failed/uncertain transfers retain their existing failure policy; a decision deadline never cancels RESTORE after chunk transmission starts.
- Control requests (`RESET`, `DRAW`, `TAKEBACK`, `RESIGN`) use bounded reply retries; the human-approval deadline above remains independent. Receivers must treat duplicates idempotently (re-ACK an already-accepted request instead of NACKing or re-prompting, and re-NACK a declined `DRAW` until the position changes).
- Mixed-version RESTORE: a peer that still treats restore as host-only answers a guest `RQ` with `RN` and does not mutate the board. Host-initiated restore remains interoperable.
- RESTORE never remaps chess colours. A local save whose `host_color` differs from the current seating is not offered; an incompatible received snapshot is answered with `RN` and leaves the active game unchanged.
- Treat unknown optional tails as advisory text unless the verb requires exact grammar.
- A control-reply target token is complete only at end of payload or before a
  space introducing its optional reason. For example, `NACK RESET BUSY` is a
  RESET reply; `NACK RESETX` is not.
- Do not send PC-only or Spectrum-only variants.
- Advisory identity payloads start with `M` (`MACH`, `MNAME`). Spectrum MQTT
  peers display unrecognized payloads as text unless their first letter is one
  of `F`, `H`, `J`, `M` or `O`, so a new advisory verb must keep that prefix.
- `MNAME` travels in clear text through the broker like every other payload;
  it is a nickname, not an authenticated identity.
- Keep MQTT `GAME START` plain for canonical output.

DRAW and RESIGN end the current game without an automatic RESET. Either role
may subsequently request RESET; the receiver asks for consent. Retransmitted
accepted DRAW requests are still ACKed while that finished game remains current.
No payload or version-negotiation change is required.

Mixed peers using the 1.2 automatic-rematch behavior retain these
limitations: after the new peer resigns, the older receiver can show
`RESTARTING GAME` until the new peer manually requests RESET, which the older
peer accepts automatically. When the older peer initiates the automatic RESET
after resignation or an accepted draw, the new Spectrum asks `S:STATS ACCEPT
RESTART? Y/N` (Qt: `Opponent restart y/n?`): S opens the statistics without
answering, and declining ends the session (NACK, then BYE). Between new peers
only the host requests the consented restart; a refusal ends the session. Physical mixed-version
validation remains pending.
