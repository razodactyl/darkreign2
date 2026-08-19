# Networking Research Notes

## Goal

Current multiplayer requires the host to be **directly reachable** — no NAT
traversal exists anywhere in the codebase. This doc maps the existing stack
closely enough to scope a **Go relay/rendezvous server** that lets firewalled
players host and join without port forwarding.

## Current Architecture

Two layers over one TCP connection, distinguished by message flags, not
separate sockets:

- **`styxnet/`** — low-level transport: custom binary packets over TCP
  (`Win32::Socket`, `win32_socket.cpp`), session/client/server objects, LAN
  discovery, ping.
- **`multiplayer/`** — lobby: session list, chat, ready state, map/mission
  negotiation, file transfer for missing maps. Built on styxnet's client API.
- **`coregame_orders/orders.cpp`** — in-game command dispatch.
- **`game/sync.cpp`** — CRC-based desync detection.

There is **no dedicated server process**. `StyxNet::Server` and
`StyxNet::Client` (`styxnet_server.h`/`.cpp`, `styxnet_client.h`/`.cpp`) are
just objects instantiated inside the same game binary — whichever player
creates the session is the "server" for that session.

### Lobby vs in-game

- **Lobby**: non-deterministic data (chat, options, map info) sent via
  `Client::SendData(key, ..., sync=FALSE)`.
- **In-game**: deterministic **lockstep**. Each client queues local player
  orders in `Orders::dispatchQueue`; `Orders::Dispatch()` batches them, stamps
  the batch with `header.gameCycle` and a sync CRC, and sends via
  `MultiPlayer::Data::Send(..., sync=TRUE)`. The host relays batches to all
  clients (`SyncStore`/`SyncClear`/`SyncFlush` events); every client executes
  the same orders at the same simulation cycle. No full game state is ever
  sent — only orders — so all clients must independently arrive at identical
  results, checked via `Sync::Test()` CRC comparison (`game/sync.cpp`).

This matters for a relay design: the relay only ever needs to move opaque
order/lobby packets between players — it does not need to understand or
reconstruct game state.

## Packet Framing

`styxnet/styxnet_packet.h:39-45`, magic confirmed in `styxnet_packet.cpp:31,372`:

```c
struct Header {
    U32 magic;    // 0xFEED2BAD
    U32 length;   // payload length
    CRC crc;      // payload checksum
    CRC command;  // opaque command id
};
U8 data[0];       // payload follows
```

16-byte header + payload, framing is length-prefixed with a magic number used
to resync the stream on corruption (`styxnet_packet.cpp:63-77`). This is good
news for a relay: it's a well-defined byte-stream boundary a Go proxy can
track without parsing payload semantics — **with one exception**, see host
migration below.

## Topology Today

### LAN discovery

`StyxNet::Explorer` (`styxnet_explorer.cpp:35-95`) broadcasts a 4-byte UDP
probe (`0x55378008`) to `INADDR_BROADCAST`; the host's explorer thread
(`styxnet_server.cpp:145-160`) answers with session data on the **same port**
as the TCP listener. LAN-only — no internet directory in styxnet itself.

### Internet discovery: WON

A separate legacy layer, `won/` (World Opponent Network — GameSpy-era SDK),
handled account login and a game/room **directory** (`woniface.h:441`
`SetDirectoryServers`, `:462` `LoginAccount`, `Game` struct at
`woniface.h:400-424`). Critically, WON was matchmaking only — it never
relayed gameplay traffic. `won_controls_gamelist.cpp:330` pings the listed
host directly to test reachability, and joining is a plain `connect()` to the
host's address:port (`multiplayer_cmd.cpp:512,534`). WON's directory servers
are long dead; this layer is effectively inert today.

### Ports

Default TCP port is `0x6666` (**26214**), `styxnet/styxnet.h:110`, with a
`0` (unset) default in user config falling back to it
(`multiplayer_settings.cpp:58,165-168`). One TCP listener per session
(`Bind`/`Listen` in `styxnet_server.cpp:146-152`), UDP used only for LAN
broadcast discovery on the same port number.

### NAT/firewall handling: none

`multiplayer_settings.cpp:41` has a `firewallStatus` field
(`FirewallStatus::AutoDetect`) but it's a **UI hint only** — never wired to
UPnP, STUN, or hole-punching logic. Grepping the whole `styxnet/`,
`multiplayer/`, `won/` trees for UPnP/NAT/punch/relay/forward turns up
nothing relevant. **The host must have the port open/forwarded today; that's
the entire problem this research is trying to solve.**

### Host migration — the one payload-aware case

`styxnet_server_migration.cpp` reassigns the host role mid-session by ping
ranking (`Server::Migration`, lines 33-50). The chosen candidate replies with
its **own local socket address embedded in the response payload**
(`styxnet_client.cpp:390-395`), which the current host then broadcasts to
everyone as `ServerMessage::SessionMigrate` so all clients reconnect directly
to the new host. This is the one place where a relay can't stay a dumb byte
pipe: the "who do I connect to" data lives inside application payloads, not
in TCP peer addresses.

## Implications for a Go Relay Server

**Good news:** normal lobby/order traffic is a clean length-prefixed binary
stream the relay never needs to parse. A store-and-forward relay is
plausible with minimal protocol knowledge.

**The catch:** host migration embeds a real IP:port inside a payload. Two
ways to avoid building a full protocol parser:

1. **Relay-as-permanent-host.** The Go server always presents itself as the
   session host (accepts the TCP listener role styxnet expects of a host) and
   never triggers/allows migration. All real players connect to the relay as
   clients; it forwards order batches between them exactly as
   `Orders::Dispatch()` → `SyncStore/Clear/Flush` already does today, just off
   a process that's always reachable instead of a player's machine. Simplest
   to build, closest to current behavior, no payload rewriting needed. Down
   side: someone has to run/pay for that always-on relay, and it doesn't help
   two firewalled players with pure P2P if that's ever wanted.

2. **Rendezvous + NAT traversal (STUN/TURN-style).** Go server does connection
   brokering only (like WON's original role, but internet-capable): parties
   exchange addresses through it, attempt UDP/TCP hole punching, fall back to
   relaying through the Go server only when direct connection fails. Requires
   switching the transport (or adding a UDP path) since hole punching is
   fundamentally easier over UDP than TCP, which is a bigger change to
   `styxnet`'s TCP-only socket layer.

Option 1 is the pragmatic near-term target — it reuses the existing lockstep
protocol almost unchanged and sidesteps the TCP-vs-UDP hole-punching problem
entirely. Option 2 is the "proper" long-term fix if peer-to-peer (no
always-on relay cost) matters.

## Open Questions / Next Steps

- Confirm whether `styxnet_client.cpp` can be pointed at a relay without
  patching the exe (i.e., does `-ip:`/session-join already accept an
  arbitrary address, or is LAN-broadcast discovery hardwired into the join
  flow for internet play too?).
- Decide whether the relay speaks raw styxnet framing (thin proxy, no
  protocol changes needed client-side) or terminates styxnet and re-emits it
  (more control, but requires reimplementing packet framing/CRC in Go).
- If migration is disabled, check whether `UserFlags::AcceptMigration` can
  simply be left unset for all real players so the relay is never offered the
  role away from itself, rather than patching migration logic out.
- Downloads/file-transfer (`multiplayer_download.cpp`) uses its own separate
  default port/HTTP-ish path — out of scope for the relay unless map
  distribution also needs to go through it.

## Summary

- **No dedicated server today** — host is just a player process; WON was
  matchmaking-only and is dead.
- **No NAT traversal exists** — direct `connect()` to the host, port must be
  open/forwarded.
- **Packets are a clean 16-byte-header binary stream** (`0xFEED2BAD` magic +
  length + CRC + command) — relayable without payload parsing, except:
- **Host migration embeds a real address in-payload** — the one case a dumb
  relay must sidestep, most simply by never allowing migration and always
  acting as host itself.
- **Recommended first step:** a Go relay that permanently plays the "host"
  role over TCP, reusing the existing lockstep protocol unmodified. STUN/TURN-
  style peer-to-peer traversal is a bigger, separate project (needs a UDP
  path in styxnet) and can follow later if avoiding an always-on relay cost
  matters.
