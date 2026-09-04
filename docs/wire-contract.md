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
| `DRAW` | Offer draw/rematch. |
| `ACK DRAW` / `NACK DRAW` | Draw response. |
| `CANCEL DRAW` | Cancel a still-pending DRAW request. The receiver answers `NACK DRAW`. |
| `RESET` | Reset/rematch request. |
| `ACK RESET` / `NACK RESET [reason]` | Reset response. |
| `CANCEL RESET` | Cancel a still-pending RESET request. The receiver answers `NACK RESET`. |
| `RESIGN` | Unilateral resignation. Sender retransmits until `ACK RESIGN` arrives. |
| `ACK RESIGN` | Resignation acknowledged. Receivers must ACK every `RESIGN`, including retransmissions. Current peers then synchronize the automatic new game through the existing `RESET` / `ACK RESET` exchange. |
| `CHAT <text>` | Chat text. Max visible text is 42 chars. |
| `MACH` + code (`ZX` / `NXT` / `MAC` / `LNX` / `PC` / `SPCX`) | Optional peer machine identity. Both peers may emit after the link is ready (`peer_ready`). Informational only: never changes game state, color, or control flow. Unknown codes and missing announcements leave the peer platform unknown. Legacy 1.1 peers ignore `MACH` silently on Direct and on Spectrum MQTT (first letter `M` is not `MQTT_TEXT`). |
| `PING` / `ACK PING` | Keepalive. |
| `BYE` | Peer disconnect. |
| `RQ` | Request permission to restore a saved position. |
| `RY` / `RN` | Accept or reject/cancel a restore exchange. |
| `RS00 <30 Base64URL chars>` | First 30 ASCII characters of the unpadded Base64URL restore encoding. Exactly 35 wire bytes including the `RS00 ` prefix. |
| `RS01 <30 Base64URL chars>` | Final 30 ASCII characters of the unpadded Base64URL restore encoding. Exactly 35 wire bytes including the `RS01 ` prefix. |
| `RA` | Snapshot applied; restore exchange complete. |

## Topic Rules

Direct carries these payloads as newline-delimited TCP lines.

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
| `CANCEL RESET` / `CANCEL DRAW` | own lateral, live, never retained | inbound lateral |
| Game payloads (`MOVE`, `TAKEBACK`, `DRAW`, `RESET`, `RESIGN`, `CHAT`, `PING`, `BYE`, `MACH`) | own lateral (`w2b` white, `b2w` black) | inbound lateral |
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

The receiver always answers `NACK RESET` or `NACK DRAW` through the normative
general NACK route. If the matching decision is pending, it is invalidated, the
NACK confirms cancellation, and a later modal decision is inert. If the request
already finished, does not match, or the CANCEL is a duplicate, the receiver
sends the same NACK without mutating state. The cancelling sender interprets
that NACK as `CANCELLED`, not as rejection of the original request.

An older peer may ignore CANCEL. While the connection remains live, the current
implementation alternates retries with five-minute grace periods; this is not
a single terminal timeout.

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

## Compatibility Rules

- Parse by payload grammar, not by peer client name.
- Control requests (`RESET`, `DRAW`, `TAKEBACK`, `RESIGN`) are retransmitted until answered; receivers must treat duplicates idempotently (re-ACK an already-accepted request instead of NACKing or re-prompting).
- Mixed-version RESTORE: a peer that still treats restore as host-only answers a guest `RQ` with `RN` and does not mutate the board. Host-initiated restore remains interoperable.
- RESTORE never remaps chess colours. A local save whose `host_color` differs from the current seating is not offered; an incompatible received snapshot is answered with `RN` and leaves the active game unchanged.
- Treat unknown optional tails as advisory text unless the verb requires exact grammar.
- Do not send PC-only or Spectrum-only variants.
- Keep MQTT `GAME START` plain for canonical output.

`RESIGN` remains wire-compatible with older peers because its acknowledgement
and the following rematch use existing payloads. A current peer automatically
sends or accepts the post-resignation `RESET`; an older peer may still expose
its legacy RESET decision prompt, so automatic rematch is guaranteed only when
both peers implement the current session contract.
