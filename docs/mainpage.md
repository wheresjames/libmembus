@mainpage libmembus API Reference

**libmembus** is a C++20 shared-memory data bus for inter-process communication.
It provides raw memory maps, message and command channels, fixed-schema key-value
state, and ring buffers for video and audio — all backed by named shared memory
with no broker process required.

Full usage documentation, design notes, and examples are in the
[README](https://github.com/wheresjames/libmembus#readme).

---

## Module overview

| Class | Role |
|---|---|
| mmb::memmap  | Raw named shared-memory buffer |
| mmb::memmsg  | Single-producer, multi-consumer message queue |
| mmb::memvid  | Lock-free video frame ring buffer |
| mmb::memaud  | Lock-free PCM audio ring buffer |
| mmb::memcmd  | Multi-producer, multi-consumer command channel |
| mmb::memkv   | Fixed-schema key-value store with seqlock reads |

## Convenience wrappers

| Class | Role |
|---|---|
| mmb::memmsg_writer / mmb::memmsg_reader   | Create/read a message queue |
| mmb::memcmd_sender / mmb::memcmd_receiver | Send/receive commands |
| mmb::memvid_writer / mmb::memvid_reader   | Publish/consume video frames |
| mmb::memaud_writer / mmb::memaud_reader   | Publish/consume audio buffers |

## Error handling

All APIs return `bool`, a numeric value, or an empty string on failure.
Use mmb::last_error() and mmb::last_error_message() to retrieve the reason.

@see mmb::errc
