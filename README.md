# libmembus

A C++20 shared memory data bus for inter-process communication. Provides raw memory maps, message and command channels, fixed-schema key-value state, and ring buffers for video and audio data — all backed by named shared memory with no broker process required.

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Building](#building)
- [Installation](#installation)
- [Design Model](#design-model)
  - [Ownership and reader model](#ownership-and-reader-model)
  - [Overrun detection and resync](#overrun-detection-and-resync)
  - [Writer restart detection](#writer-restart-detection)
  - [Reader lifecycle](#reader-lifecycle)
  - [Command channels — multiple writers](#command-channels--multiple-writers)
- [API Reference](#api-reference)
  - [memmap — raw shared memory](#memmap--raw-shared-memory)
  - [memmsg — message queue](#memmsg--message-queue)
  - [memvid — video ring buffer](#memvid--video-ring-buffer)
  - [memaud — audio ring buffer](#memaud--audio-ring-buffer)
  - [memcmd — command channel](#memcmd--command-channel)
  - [memkv — key-value store](#memkv--key-value-store)
  - [sys — signal handling](#sys--signal-handling)
- [Comparison to Similar Projects](#comparison-to-similar-projects)
- [License](#license)

---

## Features

- **`memmap`** — raw named shared memory buffer
- **`memmsg`** — single-producer, multi-consumer message queue with overrun detection
- **`memvid`** — lock-free multi-buffer video ring buffer (24-bit RGB) with overrun detection
- **`memaud`** — lock-free multi-buffer audio ring buffer (8-bit or 16-bit PCM) with overrun detection
- **`memcmd`** — multi-producer, multi-consumer broadcast command channel with overrun detection
- **`memkv`** — fixed-schema key-value store; lock-free seqlock reads, atomic batch writes, change notifications
- Single public include, compiled library implementation
- C++20, bool-returning open/write APIs
- Defensive validation of shared-memory headers before attaching to existing structured shares

---

## Requirements

- CMake 3.30+
- C++20 compiler
- Ninja (recommended) or Make
- Boost (`stacktrace_backtrace` component, plus Boost.Interprocess and Boost.DateTime)
- POSIX-style named shared memory is the primary tested target. The process-boundary smoke test is enabled on Unix-like platforms.

---

## Building

```bash
# Configure (first time only)
cmake . -B ./build -G Ninja

# Build and test
cmake --build ./build --parallel
```

To run tests manually:

```bash
ctest --test-dir ./build --output-on-failure
```

Run a specific test group:

```bash
ctest --test-dir ./build -R "MemMap"
ctest --test-dir ./build -R "MessageQueue"
ctest --test-dir ./build -R "MemVid"
ctest --test-dir ./build -R "MemAud"
ctest --test-dir ./build -R "MemCmd"
ctest --test-dir ./build -R "MemKV"
ctest --test-dir ./build -R "ipc_smoke"
```

---

## Installation

```bash
cmake --install ./build
```

Installs the library to `lib/` and headers to `include/`.

---

## Design Model

### Ownership and reader model

The process that creates a share owns its OS namespace entry and removes it on `close()`. Processes that attach to an existing share do not remove it on close.

`memvid`, `memaud`, and `memmsg` are designed around one publishing writer and any number of independent readers. Readers never need to coordinate with each other.

```
Writer process                  Reader process A
  memvid::open(bCreate=true)      memvid::open_existing(name)
  next() / next() / ...           reads frames independently

                                Reader process B
                                  memvid::open_existing(name)
                                  reads frames independently
```

**`memvid` and `memaud`** use a fully lock-free ring buffer. The writer advances an atomic pointer via `next()`; readers observe that pointer and the per-frame sequence numbers independently without any synchronisation between readers.

**`memmsg`** uses an interprocess mutex and condition variable so readers can block-wait for messages. All readers share the same mutex but each maintains its own private read position, so every reader receives every message independently (broadcast, not work-stealing).

`memcmd` intentionally supports multiple writers and multiple registered readers. `memkv` allows any open handle to write values after the owner has created the fixed schema.

### Overrun detection and resync

The writer does not slow down or block for slow readers. If a reader falls behind by more than `getBufs()` frames (video/audio) or the write pointer laps the read position (messages), the reader is overrun. The library provides the tools to detect this reliably.

#### `memvid` / `memaud` — sequence-number lag check

`next()` atomically increments a global sequence counter in the shared header and stamps the current slot's frame header with that counter before advancing the write pointer. Readers never need to agree on a lock; they simply compare counters.

```cpp
int64_t rPos     = vid.getPtr(0);   // slot the writer will write next
int64_t rLastSeq = vid.getSeq();    // treat everything written so far as seen

while (running)
{
    int64_t lag = vid.getSeq() - rLastSeq;

    if (lag == 0)
    {
        // Nothing new yet — sleep or spin
        continue;
    }

    if (lag >= vid.getBufs())
    {
        // Writer lapped us.  Discard stale position and jump forward.
        rPos     = vid.getPtr(0);
        rLastSeq = vid.getSeq();
        // Handle dropped-frame event here if needed
        continue;
    }

    // 1 <= lag < getBufs(): data is available and the ring is intact.
    // Optionally verify the exact slot before copying:
    bool in_sync = (vid.getFrameSeq(rPos) == rLastSeq + 1);  // no gaps

    auto frame = vid.getBuf(rPos);
    // ... process frame.m_ptr ...

    rLastSeq = vid.getFrameSeq(rPos);
    rPos     = (rPos + 1) % vid.getBufs();
}
```

For tear-free reads, bracket the copy with a before/after sequence check:

```cpp
int64_t seq_before = vid.getFrameSeq(rPos);
auto view = vid.getBuf(rPos);
// ... copy pixels from view.m_ptr ...
bool torn = (vid.getFrameSeq(rPos) != seq_before);
if (torn) { /* discard and retry next frame */ }
```

#### `memmsg` — sequence-number gap check

`write()` stamps each message frame with a monotonically increasing sequence number. `read()` compares it against the last-seen sequence; a gap signals overrun. On overrun `read()` resyncs the reader's position to the current write position and returns an empty string with `*pOverrun = true`.

```cpp
mmb::memmsg rx;
rx.open("/my_queue", 1024, /*bWrite=*/false, /*bCreate=*/false);

bool overrun = false;
std::string msg = rx.read(/*wait_ms=*/100, &overrun);

if (overrun)
{
    // One or more messages were skipped; reader has been resynced.
    // Call read() again to receive the next message.
}
else if (!msg.empty())
{
    // Process msg normally.
}
```

### Writer restart detection

When the writer calls `close()` (or its process exits) the shared memory is removed from the OS namespace. Readers that were already attached keep their memory-mapped view of the old data — the map remains valid until the reader calls `close()`, but it will never be updated again.

When the writer restarts, it creates a **new** share at the same name with a fresh random session ID. Readers with stale maps will not see this automatically.

**Detection pattern** — readers should save the session ID on open and re-check it periodically. If the ID changes or `open_existing()` fails, reconnect.

```cpp
mmb::memvid vid;
if (!vid.open_existing("/my_video"))
    return; // writer not yet running

int64_t sessionId = vid.getSessionId();
int64_t rLastSeq  = vid.getSeq();
int64_t rPos      = vid.getPtr(0);

while (running)
{
    // Periodically (e.g. every second, or when getSeq() stops advancing):
    {
        mmb::memvid probe;
        if (!probe.open_existing("/my_video") ||
             probe.getSessionId() != sessionId)
        {
            // Writer restarted (or share is gone). Reconnect.
            vid.close();
            if (!vid.open_existing("/my_video"))
                break; // writer still gone, retry later
            sessionId = vid.getSessionId();
            rLastSeq  = vid.getSeq();
            rPos      = vid.getPtr(0);
        }
    }

    // Normal read loop ...
}
```

### Command channels — multiple writers

`memcmd` reverses the data-flow direction: multiple consumer processes write commands, and the capture process reads them. This is the typical pattern for camera control (pan, tilt, zoom) where any viewer may send a command at any time.

```
Consumer A ──write("pan_left")──►
Consumer B ──write("pan_stop")──► [memcmd ring buffer] ──read()──► Capture process
Consumer C ──write("zoom_in") ──►
```

Concurrent writers are serialised by the shared interprocess mutex. Every registered reader receives every command independently (broadcast). Overrun detection and resync work identically to `memmsg`.

The capture process creates the channel and registers as a reader (`bReader=true, bCreate=true`). Consumer processes attach and write without registering as readers (`bReader=false, bCreate=false`). Any open handle — reader-registered or not — may call `write()`.

See the [`memcmd` API reference](#memcmd--command-channel) for the full API.

### Reader lifecycle

| Event | Behaviour |
|---|---|
| Reader attaches while writer runs | `open_existing()` maps to the live share; start with `rLastSeq = getSeq()` and `rPos = getPtr(0)` |
| Reader falls behind by < `getBufs()` frames | Data is still in the ring; reader can catch up or skip |
| Reader is lapped (`lag >= getBufs()`) | Detected via sequence check; reader resyncs and signals caller |
| Reader disconnects cleanly | `close()` unmaps memory; share is not removed (reader opened with `m_bExisting = true`) |
| Reader crashes | Same as clean disconnect from the writer's perspective |
| Writer stops cleanly | Share is removed from namespace; readers' maps go stale; `getSeq()` stops advancing |
| Writer crashes | Share lingers in namespace; `getSeq()` stops advancing; next writer restart removes the stale share and creates a fresh one |
| Writer restarts | New share, new session ID; existing readers detect via `getSessionId()` change and reconnect |

---

## API Reference

All types live in the `mmb` namespace. Include the single top-level header:

```cpp
#include "libmembus.h"
```

---

### memmap — raw shared memory

```cpp
mmb::memmap writer, reader;

// Create a 1 KB shared memory region
writer.open("/my_share", 1024, /*bCreate=*/true, /*bNew=*/true);

// Attach from another process
reader.open("/my_share", 1024, /*bCreate=*/false, /*bNew=*/false);

// Write and read raw strings
writer.write("hello");
std::string data = reader.read(5);  // read up to 5 bytes
std::string all  = reader.read();   // read entire buffer

// Inspect state
writer.isOpen();    // true
reader.existing();  // true — share already existed when opened
writer.size();      // 1024
writer.name();      // "/my_share"
char* ptr = writer.data();  // raw pointer
```

`open()` parameters:

| Parameter | Description |
|-----------|-------------|
| `sName`   | Share name (POSIX: must start with `/`) |
| `nSize`   | Size in bytes |
| `bCreate` | Create if it does not exist |
| `bNew`    | Unlink and recreate if it already exists |

The process that created the share (`existing() == false`) owns it and removes it from the namespace on `close()`. Processes that attached to an existing share (`existing() == true`) do not remove it on close.

Opening an existing share with `bCreate=true` does not resize it. `nSize` is used to size newly-created shares; attached handles report the actual mapped size via `size()`.

---

### memmsg — message queue

Single-producer, multi-consumer broadcast queue. Every reader receives every message independently. The writer opens with `bWrite=true`; readers open with `bWrite=false`.

```cpp
mmb::memmsg tx, rx;

tx.open("/my_queue", 1024, /*bWrite=*/true,  /*bCreate=*/true);
rx.open("/my_queue", 1024, /*bWrite=*/false, /*bCreate=*/false);

tx.write("hello");

// Blocking read — wait up to 100 ms
bool overrun = false;
std::string msg = rx.read(100, &overrun);

// Non-blocking read
std::string msg2 = rx.read(0);
```

**`write(msg)`** returns `false` if the message is empty or too large for the buffer.

**`read(wait_ms, pOverrun)`**

| Return | Meaning |
|---|---|
| Non-empty string, `*pOverrun = false` | Message received normally |
| Empty string, `*pOverrun = false` | Timed out with no message |
| Empty string, `*pOverrun = true` | Reader was lapped; position resynced — call `read()` again |

Notes:
- Both sides must open with the same `size` or the attach will fail.
- Attach also fails if an existing share is too small for the requested queue layout.
- The internal mutex is acquired with a 5-second timeout; if the writer crashes holding the lock, readers will surface an error rather than blocking forever.

---

### memvid — video ring buffer

Lock-free ring buffer of raw 24-bit RGB frames. The writer calls `next()` to publish each frame; readers observe the write pointer and per-frame sequence numbers independently.

```cpp
mmb::memvid producer, consumer;

// Create: 1920x1080, 24bpp, 30fps, 4-frame ring buffer
producer.open("/my_video", /*bCreate=*/true, 1920, 1080, 24, 30, /*bufs=*/4);

// Attach from another process (with known parameters)
consumer.open("/my_video", false, 1920, 1080, 24, 30, 4);

// Attach without knowing the parameters
consumer.open_existing("/my_video");

// Write: fill slot 0 with solid colour, then publish and advance pointer
producer.fill(0, 0xFF);
producer.next(1);

// Read: get the most recently completed frame
int64_t rPos = consumer.getPtr(-1);  // slot written just before the write pointer
mmb::memvid::vidview frame = consumer.getBuf(rPos);
// frame.m_ptr  — raw pixel data
// frame.m_w    — width
// frame.m_h    — height
// frame.m_sw   — scan width (bytes per row)
// frame.m_size — total bytes (m_sw * m_h)
```

`open_existing()` validates the header and rejects malformed or undersized shares before exposing buffer views.

**Pointer and sequence helpers:**

| Method | Description |
|--------|-------------|
| `setPtr(p)` | Set the write pointer to `p` (wrapped); returns `p` |
| `getPtr(offset)` | Return `(ptr + offset) % bufs` |
| `next(inc)` | Stamp current slot's sequence, advance pointer by `inc` |
| `getPtrErr(pos, bias)` | Signed circular distance from `ptr+bias` to `pos` |
| `getSeq()` | Global write-sequence counter (incremented by every `next()`) |
| `getFrameSeq(idx)` | Sequence number stamped in slot `idx`; 0 means never written |
| `getSessionId()` | Random ID written at share creation; changes on every writer restart |

**Metadata:**

| Method | Description |
|--------|-------------|
| `getWidth()` | Frame width in pixels |
| `getHeight()` | Frame height in pixels |
| `getBpp()` | Bits per pixel (always 24) |
| `getFps()` | Frames per second |
| `getBufs()` | Number of slots in the ring |

---

### memaud — audio ring buffer

Same lock-free ring-buffer model as `memvid` but for PCM audio buffers.

```cpp
mmb::memaud producer, consumer;

// Create: stereo, 16-bit, 44100 Hz sample rate, 30 buffers/sec, 3-buffer ring
producer.open("/my_audio", /*bCreate=*/true,
              /*ch=*/2, /*bps=*/16, /*bitrate=*/44100, /*fps=*/30, /*bufs=*/3);

consumer.open("/my_audio", false, 2, 16, 44100, 30, 3);
// Or:
consumer.open_existing("/my_audio");

// Write
producer.fill(0, 0x00);  // silence
producer.next(1);

// Read
mmb::memaud::audview buf = consumer.getBuf(consumer.getPtr(-1));
// buf.m_ptr  — raw sample data
// buf.m_size — size in bytes
// buf.m_ch   — channels
// buf.m_bps  — bits per sample
```

`open_existing()` validates the header and rejects malformed or undersized shares before exposing buffer views.

**Metadata:**

| Method | Description |
|--------|-------------|
| `getChannels()` | Number of channels |
| `getBps()` | Bits per sample (8 or 16) |
| `getBitRate()` | Sample rate in Hz |
| `getFps()` | Buffers per second |
| `getBufs()` | Number of slots in the ring |
| `getBufSize()` | Bytes per buffer |

The pointer/sequence/session helpers (`setPtr`, `getPtr`, `next`, `getPtrErr`, `getSeq`, `getFrameSeq`, `getSessionId`) work identically to the `memvid` equivalents.

Supported `bps` values: `8` or `16`.

---

### memcmd — command channel

Multi-producer, multi-consumer broadcast command channel. Any open handle may write; every registered reader receives every message independently. Designed for the reverse data-flow case: consumer processes send control commands to the capture process.

```cpp
// Capture process — creates the channel and registers as a reader
mmb::memcmd cmd;
cmd.open("/cam_commands", 4096, /*bReader=*/true, /*bCreate=*/true);

while (running) {
    bool overrun = false;
    std::string c = cmd.read(/*wait_ms=*/100, &overrun);
    if (overrun) { /* some commands were skipped */ continue; }
    if (c == "pan_left")  camera.pan(-1);
    if (c == "pan_stop")  camera.stop();
}

// Consumer process — attaches and sends a command (defaults: bReader=false, bCreate=false)
mmb::memcmd cmd;
if (cmd.open("/cam_commands", 4096))
    cmd.write("pan_left");
```

**`open(sName, size, bReader, bCreate)`**

| Parameter | Description |
|-----------|-------------|
| `sName`   | Share name (POSIX: must start with `/`) |
| `size`    | Ring buffer capacity in bytes |
| `bReader` | Register as a reader (increments `readerCount()`); default `false` |
| `bCreate` | Create the share if it does not exist; default `false` |

The process that creates the share owns it and removes it from the OS namespace on `close()`. Both sides must open with the same `size` or the attach will fail.
Attach also fails if an existing share is too small for the requested command-channel layout.

**`write(msg)`** — returns `false` if the payload is empty, too large for the buffer, or the mutex could not be acquired within 5 seconds (crash recovery).

**`read(wait_ms, pOverrun)`**

| Return | Meaning |
|---|---|
| Non-empty string, `*pOverrun = false` | Command received normally |
| Empty string, `*pOverrun = false` | Timed out with no command |
| Empty string, `*pOverrun = true` | Reader was lapped; position resynced — call `read()` again |

**`readerCount()`** — number of handles currently opened with `bReader=true`. Treat as a hint; may be temporarily stale if a reader crashed before calling `close()`.

---

### memkv — key-value store

Fixed-schema shared memory key-value store. The owner creates it with a slot count and maximum name/value lengths; those are immutable for the lifetime of the share. Any process may read or write values after attaching.

**Writes** are serialised by an interprocess mutex. **Reads** are lock-free via a per-slot seqlock: readers never acquire any lock. If a write is in progress on a slot the reader retries (nanoseconds per retry); a stuck seqlock after 1000 retries sets `*pStale = true` to signal a possible writer crash.

```cpp
// Owner — creates the store and populates names before publishing
mmb::memkv kv;
kv.create("/cam_state", 4, /*maxNameLen=*/31, /*maxValueLen=*/63);
kv.setName(0, "pan");   kv.setValue(0, "0");
kv.setName(1, "tilt");  kv.setValue(1, "0");
kv.setName(2, "zoom");  kv.setValue(2, "1.0");
kv.setName(3, "focus"); kv.setValue(3, "auto");

// Write a single value
kv.setValue("pan", "-15");

// Write multiple values atomically (one epoch increment, one notify)
kv.setAll({{"pan", "-15"}, {"tilt", "5"}, {"zoom", "1.4"}});

// Any other process
mmb::memkv reader;
reader.open("/cam_state");

// Lock-free read
std::string pan = reader.getValue("pan");

// Consistent snapshot of all entries
auto all = reader.getAll();

// Poll for changes (non-blocking)
int64_t epoch = reader.getEpoch();
auto changed = reader.getChanged(epoch);    // returns map of changed entries, updates epoch

// Wait for changes (blocking)
changed = reader.getChanged(100, epoch);    // blocks up to 100 ms
```

**`create(sName, count, maxNameLen, maxValueLen, bNew=false)`** — creates the share; optionally removes any stale share first with `bNew=true`.

**`open(sName)`** — attaches to an existing share. Reads schema (count, name/value limits) from the header and rejects malformed or undersized layouts.

**`setName(idx, name)`** — sets the immutable name for slot `idx`. Call only before other processes attach.

**`setValue(idx|name, value)`** — mutex-protected write to a single slot. Returns false if the value exceeds `maxValueLen` or the lock times out (5 s, crash recovery).

**`setAll(map)`** — writes every entry under one mutex acquisition. All slots change atomically. Names not in the store are silently skipped.

**`getValue(idx|name, pStale)`** — lock-free seqlock read. `pStale` is set true if the seqlock was stuck (writer crash indicator).

**`getAll()`** — epoch-checked consistent snapshot: retries until a complete pass finishes with no concurrent write.

**`getChanged(epoch)`** — non-blocking; returns a map of every slot whose value changed since `epoch`; updates `epoch` to the current value.

**`getChanged(wait_ms, epoch)`** — blocking; waits up to `wait_ms` for any change, then returns changed slots. Empty map on timeout.

**`waitForChange(wait_ms, epoch)`** — blocks until epoch advances; returns true if a change occurred, false on timeout.

**Owner crash recovery** — `getValue()` caps retries at 1000 (sets `*pStale = true` if stuck). `waitForChange()` / `setValue()` / `setAll()` fail with false after a 5 s lock timeout. Application-level recovery: monitor `waitForChange()` timeouts, then `close()` and `open()` to reconnect.

---

### sys — signal handling

```cpp
static volatile int ctrl_c_count = 0;
mmb::install_ctrl_c_handler(&ctrl_c_count);

while (!ctrl_c_count)
{
    // do work
}
// pressing Ctrl-C three times exits immediately
```

`ctrl_c_count` is incremented each time Ctrl-C is pressed. After three presses the process exits immediately.

---

## Comparison to Similar Projects

Several projects solve shared-memory IPC in different ways. The right choice depends on whether you need a small embeddable C++ library, a full middleware stack, durable queues, or low-level shared-memory primitives.

---

### Boost.Interprocess

Boost.Interprocess is the closest low-level foundation. It provides shared memory, mapped files, named synchronization primitives, process-shared mutexes and condition variables, message queues, and shared-memory allocators/containers.

Key differences:

- Boost.Interprocess is a general-purpose toolkit. libmembus is a higher-level set of fixed IPC patterns built on Boost.Interprocess-style primitives.
- Boost.Interprocess gives you raw mechanisms; you design your own message framing, overrun handling, restart detection, and media/state layouts.
- libmembus provides ready-made `memmsg`, `memcmd`, `memvid`, `memaud`, and `memkv` abstractions with tests for process-boundary behavior.
- Boost.Interprocess is more flexible and portable; libmembus is smaller and more opinionated.

Choose Boost.Interprocess if you need maximum control over the shared-memory layout or want STL-like containers and allocators in shared memory.

Choose libmembus if you want a compact library with predefined message, command, media-ring, and key-value patterns.

---

### cpp-ipc / libipc

cpp-ipc is a high-performance C++ IPC library using shared memory and circular buffers. It supports single-writer/multi-reader routes and multi-reader/multi-writer channels with broadcast-style delivery.

Key differences:

- cpp-ipc is closest to `memmsg` and `memcmd`: shared-memory channels, circular buffers, timeouts, and broadcast delivery.
- cpp-ipc is more focused on generic message channels. libmembus also includes typed-ish domain buffers for video, audio, and fixed-schema key-value state.
- cpp-ipc has broader packaged ecosystem support through package managers such as vcpkg and Conan.
- libmembus currently has a smaller API surface and fewer dependencies beyond Boost.

Choose cpp-ipc if you mainly need high-performance generic IPC channels across several platforms.

Choose libmembus if your application also needs shared video/audio frame rings or simple shared process state.

---

### Eclipse iceoryx

Eclipse iceoryx is a true zero-copy shared-memory IPC middleware designed for high-throughput publish/subscribe systems, especially robotics, automotive, and real-time applications.

Key differences:

- iceoryx is middleware with service discovery, publishers/subscribers, and integration paths for larger ecosystems such as ROS 2 and AUTOSAR Adaptive.
- libmembus is a small library with named shares and no daemon, discovery service, or framework-level routing.
- iceoryx is designed for large zero-copy data flows and stricter safety-oriented environments.
- libmembus is easier to inspect and embed when a few named local process channels are enough.

Choose iceoryx if you need a production middleware layer, discovery, many publishers/subscribers, and strong zero-copy semantics.

Choose libmembus if you want direct named shared-memory channels with minimal infrastructure.

---

### Eclipse eCAL

Eclipse eCAL is a communication middleware with publish/subscribe, client/server, shared-memory transport, network transports, recording/replay tools, and multiple language bindings.

Key differences:

- eCAL supports interprocess and interhost communication. libmembus is local shared-memory IPC.
- eCAL includes tooling, message protocol support, and a larger runtime model.
- libmembus has no broker, recorder, network transport, or schema framework.
- eCAL is a better fit for distributed systems; libmembus is a better fit for local process coordination.

Choose eCAL if you need a full communication abstraction layer with tooling, network support, and multi-language integration.

Choose libmembus if all communication is local and you want simple named shared-memory objects.

---

### Flow-IPC

Flow-IPC is a modern C++ toolkit for high-speed IPC, including zero-copy data sharing, message queues, sessions, and structured serialization-oriented workflows.

Key differences:

- Flow-IPC is a broader toolkit with session management and more infrastructure around transmission of data structures.
- libmembus exposes simpler fixed abstractions and leaves lifecycle orchestration to the application.
- Flow-IPC is more appropriate when IPC is a major architectural layer.
- libmembus is more appropriate when IPC should stay as a small utility library.

Choose Flow-IPC if you need a comprehensive C++ IPC toolkit with session-oriented design and zero-copy object transfer.

Choose libmembus if you need a small set of practical shared-memory building blocks.

---

### Chronicle Queue

Chronicle Queue is a memory-mapped, persisted, low-latency messaging system. It is log-oriented: messages are recorded and can be replayed.

Key differences:

- Chronicle Queue is durable and replayable. libmembus is volatile shared memory; data disappears when the owning share is removed.
- Chronicle Queue is built around append-only queues and persistence to disk.
- libmembus is built around live shared state, ring buffers, and latest-data delivery.
- Chronicle Queue is useful when auditability or replay matters; libmembus is useful when live low-overhead sharing matters.

Choose Chronicle Queue if you need persistent IPC, replay, or a durable event log.

Choose libmembus if you only need live local shared memory with no persistence layer.

---

### libsharedmemory

libsharedmemory is a small cross-platform C++ wrapper for low-level shared-memory buffers.

Key differences:

- libsharedmemory is closest to `memmap`: raw shared-memory access with a small API.
- libmembus adds higher-level protocols on top of raw mapping: message queues, command channels, media rings, and key-value state.
- libsharedmemory is smaller if raw mapped bytes are all you need.
- libmembus carries more behavior and therefore more assumptions.

Choose libsharedmemory if your application already owns the protocol and only needs a shared-memory wrapper.

Choose libmembus if you want reusable IPC patterns rather than only a mapped byte span.

---

### Summary

| Project | Best fit | Brokerless local SHM | Message channels | Media rings | Key-value state | Persistence | Middleware/tooling |
|---|---|---:|---:|---:|---:|---:|---:|
| libmembus | Small local C++ IPC with fixed patterns | Yes | Yes | Yes | Yes | No | No |
| Boost.Interprocess | Low-level shared-memory primitives | Yes | Yes | No | Build yourself | No | No |
| cpp-ipc | High-performance generic IPC channels | Yes | Yes | No | No | No | No |
| iceoryx | Zero-copy pub/sub middleware | Yes | Pub/sub | Generic payloads | No | No | Yes |
| eCAL | Distributed pub/sub and tooling | Uses SHM locally | Pub/sub | Generic payloads | No | Recording support | Yes |
| Flow-IPC | Full C++ IPC toolkit | Yes | Yes | Generic payloads | No | No | Some |
| Chronicle Queue | Durable low-latency event log | Memory-mapped files | Queue/log | No | No | Yes | Some |
| libsharedmemory | Raw shared-memory wrapper | Yes | Build yourself | Build yourself | Build yourself | No | No |

---

## License

MIT — see [LICENSE](LICENSE).
