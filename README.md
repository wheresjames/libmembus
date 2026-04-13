# libmembus

A C++20 shared memory data bus for inter-process communication. Provides raw memory maps, a message queue, and ring buffers for video and audio data — all backed by named shared memory regions with no broker process required.

## Features

- **`memmap`** — raw named shared memory buffer
- **`memmsg`** — single-producer / single-consumer message queue
- **`memvid`** — multi-buffer video ring buffer (24-bit RGB)
- **`memaud`** — multi-buffer audio ring buffer (8-bit or 16-bit PCM)
- POSIX and Windows support
- Header-only API, single `#include`
- C++20, no exceptions required at call sites

## Requirements

- CMake 3.30+
- C++20 compiler (GCC, Clang, MSVC)
- Ninja (recommended) or Make
- Boost (`stacktrace_backtrace` component)

## Building

```bash
# Configure (first time only)
cmake . -B ./build -G Ninja

# Build
cmake --build ./build --parallel
```

Tests run automatically after each build. To run them manually:

```bash
ctest --test-dir ./build --output-on-failure
```

Run a specific test group:

```bash
ctest --test-dir ./build -R "MemMap"
ctest --test-dir ./build -R "MessageQueue"
ctest --test-dir ./build -R "MemVid"
ctest --test-dir ./build -R "MemAud"
```

## Installation

```bash
cmake --install ./build
```

Installs the library to `lib/` and headers to `include/`.

## Usage

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

// Read entire buffer
std::string all = reader.read();

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
| `sName`   | Share name (POSIX: starts with `/`) |
| `nSize`   | Size in bytes |
| `bCreate` | Create if it does not exist |
| `bNew`    | Unlink and recreate if it already exists |

---

### memmsg — message queue

Single-producer, single-consumer queue. One side opens with `bWrite=true`, the other with `bWrite=false`.

```cpp
mmb::memmsg tx, rx;

tx.open("/my_queue", 1024, /*bWrite=*/true,  /*bCreate=*/true);
rx.open("/my_queue", 1024, /*bWrite=*/false, /*bCreate=*/false);

tx.write("hello");

std::string msg = rx.read(/*wait_ms=*/100);  // wait up to 100ms
// msg == "hello"

// Non-blocking read
std::string msg2 = rx.read(0);  // returns "" if nothing available
```

Notes:
- Only the writer side may call `write()`; writing from a reader returns `false`.
- Writing an empty string returns `false`.
- Both sides must open with the same `size` or the attach will fail.

---

### memvid — video ring buffer

Shares a circular array of raw 24-bit RGB frames between processes. The producer writes frames and advances a pointer; consumers read at an offset behind the pointer.

```cpp
mmb::memvid producer, consumer;

// Create: 1920x1080, 24bpp, 30fps, 4-frame ring buffer
producer.open("/my_video", /*bCreate=*/true, 1920, 1080, 24, 30, /*bufs=*/4);

// Attach from another process
consumer.open("/my_video", false, 1920, 1080, 24, 30, 4);
// Or attach without knowing the parameters:
consumer.open_existing("/my_video");

// Write: fill frame 0 with solid colour, then advance pointer
producer.fill(0, 0xFF);
producer.next(1);

// Read: get the frame 1 behind the current write pointer
mmb::memvid::vidview frame = consumer.getBuf(consumer.getPtr(1));
// frame.m_ptr  — raw pixel data
// frame.m_w    — width
// frame.m_h    — height
// frame.m_sw   — scan width (bytes per row)

// Metadata
producer.getWidth();   // 1920
producer.getHeight();  // 1080
producer.getBpp();     // 24
producer.getFps();     // 30
producer.getBufs();    // 4
```

**Pointer helpers:**

| Method | Description |
|--------|-------------|
| `setPtr(p)` | Set the write pointer to `p`, returns `p` |
| `getPtr(offset)` | Return `(ptr + offset) % bufs` |
| `next(inc)` | Advance pointer by `inc`, return new value |
| `getPtrErr(pos, bias)` | Circular distance from `ptr+bias` to `pos` |

---

### memaud — audio ring buffer

Same ring-buffer model as `memvid` but for PCM audio.

```cpp
mmb::memaud producer, consumer;

// Create: stereo, 16-bit, 44100 Hz, 30fps, 3-frame ring buffer
producer.open("/my_audio", /*bCreate=*/true,
              /*ch=*/2, /*bps=*/16, /*bitrate=*/44100, /*fps=*/30, /*bufs=*/3);

consumer.open("/my_audio", false, 2, 16, 44100, 30, 3);
// Or:
consumer.open_existing("/my_audio");

// Fill buffer 0 with silence and advance
producer.fill(0, 0x00);
producer.next(1);

// Read
mmb::memaud::audview buf = consumer.getBuf(consumer.getPtr(1));
// buf.m_ptr  — raw sample data
// buf.m_size — size in bytes
// buf.m_ch   — channels
// buf.m_bps  — bits per sample

// Metadata
producer.getChannels();  // 2
producer.getBps();       // 16
producer.getBitRate();   // 44100
producer.getFps();       // 30
producer.getBufs();      // 3
producer.getBufSize();   // bytes per buffer
```

Supported `bps` values: `8` or `16`.

---

### sys — signal handling

```cpp
static volatile int ctrl_c_count = 0;
mmb::install_ctrl_c_handler(&ctrl_c_count);

while (!ctrl_c_count)
{
    // do work
}
```

`ctrl_c_count` is incremented each time Ctrl-C is pressed.

---

## License

MIT — see [LICENSE](LICENSE).
