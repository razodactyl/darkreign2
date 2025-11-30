# MINTCLIENT / MINTSOFT Networking Documentation

## Overview

MINTCLIENT is the networking layer for Dark Reign 2's multiplayer functionality, replacing the original WON (World Opponent Network) backend with a custom implementation connecting to MINTSOFT servers.

## Architecture

### Key Classes

- **`MINTCLIENT::Client`** - Main client class managing network connections and command queues
- **`MINTCLIENT::Client::MINTCommand`** - Represents a command sent to/from the server
- **`RoutingServerClient`** - Handles routing server connections for lobby/chat functionality

### Threading Model

The networking code is multi-threaded:
- Main game thread queues commands and processes callbacks
- Network threads handle socket I/O and packet processing
- Critical sections (`commandsCrit`) protect shared data structures

## Issues Fixed

### Race Condition: Double-Drop of Commands (FIXED)

**Symptom**: Crash with `read access violation` where `**ptr** was 0xDDDDDDDD` (MSVC freed memory pattern)

**Root Cause**: Multiple threads could call `DropCommand()` on the same `MINTCommand` simultaneously. The command would be unlinked from the list by one thread, then another thread would try to access the freed memory.

**Evidence from logs**:
```
[0000173Ch] MINTCLIENT::Client::DropCommand -> [910DB9D4h] -> dropped!
[00002998h] MINTCLIENT::Client::DropCommand -> [910DB9D4h] -> dropped!
[0000173Ch] MINTCLIENT::Client::DropCommand -> dropping [910DB9D4h] from this client...
Exception thrown: read access violation.
```

**Fix 1 - Critical Section in DropCommand** (`MINTCLIENT.cpp`):
```cpp
void Client::DropCommand(MINTCommand* command)
{
    commandsCrit.Enter();
    
    // Check if command is still in the list before unlinking
    Bool found = FALSE;
    for (U32 i = 0; i < this->commands.GetCount(); i++)
    {
        if (this->commands[i] == command)
        {
            found = TRUE;
            break;
        }
    }
    
    if (found)
    {
        this->commands.Unlink(command);
    }
    
    commandsCrit.Exit();
}
```

**Fix 2 - did_drop Flag** (`MINTCLIENT.h`):
Added `bool did_drop` flag to `MINTCommand` struct to prevent `DropFromClient()` from being called multiple times:
```cpp
void DropFromClient()
{
    ASSERT(client);
    
    // Prevent double-drop from multiple threads
    if (did_drop)
    {
        return;
    }
    did_drop = true;
    
    client->DropCommand(this);
}
```

**Files Modified**:
- `wonclient/MINTCLIENT.cpp` - Added critical section protection to `DropCommand()`
- `wonclient/MINTCLIENT.h` - Added `did_drop` flag and check in `DropFromClient()`

## Command Lifecycle

1. **Creation**: Command is created with `new MINTCommand()`
2. **Queue**: Command is queued via `Client::QueueCommand()` (protected by `commandsCrit`)
3. **Send**: Command is sent via `AttemptSend()` which sets `did_send = true`
4. **Process**: Server response is processed, `Done()` sets `did_complete = true`
5. **Drop**: Command is dropped via `DropFromClient()` which sets `did_drop = true` and calls `Client::DropCommand()`
6. **Cleanup**: Command is deleted

## Command Flags

| Flag | Purpose |
|------|---------|
| `did_pass` | Command has been passed to a client |
| `did_send` | `Send` function has been called |
| `did_complete` | `done` signal received |
| `did_abort` | `abort` signal received |
| `did_timeout` | `timeout` signal received |
| `did_drop` | Command has been dropped from client (prevents double-drop) |

## Critical Sections

- **`commandsCrit`** - Protects the `commands` list in `Client`
  - Used in: `QueueCommand()`, `DropCommand()`, `GetCommandById()`

## Known Issues / TODO

- Consider using atomic operations for flag checks instead of relying solely on critical sections
- The `did_drop` check in `DropFromClient()` is not thread-safe (race between check and set) - may need atomic compare-and-swap for complete safety
