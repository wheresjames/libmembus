# Bug and Security Fix Report

This document summarises every issue found in the libmembus codebase and the
corresponding fix applied.  All fixes compile cleanly and all 10 tests pass.

---

## Bugs

### B1 — `memaud_writer::open` silently ignored `bNew` parameter
**File:** `include/libmembus/helpers.h`

**Problem:** `memaud_writer::open(…, bool bNew = true)` accepted `bNew` but
never passed it anywhere; `memaud::open` already removes and recreates the share
whenever `bCreate=true`.  A caller passing `bNew=false` expected to attach to an
existing share but instead silently destroyed it.

**Fix:** Removed the vestigial `bNew` parameter from `memaud_writer::open`.
The `memmsg_writer` and `memcmd_receiver` wrappers that explicitly call
`remove()` before opening already handle their own `bNew` semantics correctly
and were not changed.

---

### B2 — Unaligned `int64_t` frame headers in `memmsg` and `memcmd` ring buffers
**Files:** `source/cpp/memmsg.cpp`, `source/cpp/memcmd.cpp`

**Problem:** Each ring-buffer frame is `fv_last (16 bytes) + len bytes`.  When
`len` is not a multiple of 8, the next frame's `int64_t` header fields
(`fv_size`, `fv_seq`) sit at an unaligned address.  This is undefined behaviour
per the C++ standard and causes bus errors on strict-alignment architectures
(SPARC, some MIPS/ARM configurations).

**Fix:** Introduced `frameStride(len)` — rounds the total frame size up to the
next 8-byte multiple:
```cpp
inline int64_t frameStride(int64_t len)
{
    return (fv_last + len + 7) & ~int64_t(7);
}
```
Both the write-advance (`*pWrite += stride`) and the read-advance
(`m_nRead += frameStride(len)`) use this function.  The wrap condition was
updated from `>=` to `>` to accommodate the aligned stride.

The minimum backing-size overhead was updated from `2 * fv_last` to
`2 * (fv_last + 7)` to account for the worst-case 7 bytes of alignment padding
per frame.

**Wire-format note:** This changes the on-disk/shared-memory frame layout.
Existing live shares created by the old code must be closed and reopened after
upgrading.

---

### B3 — `memaud::calcLayout` truncated `samplesPerFrame`, causing slow clock drift
**File:** `source/cpp/memaud.cpp`

**Problem:** `samplesPerFrame = sampleRate / fps` truncates the result.  For
sample rates that are not exact multiples of the frame rate (e.g., 44100 / 11 =
4009.09…), each frame buffer is one sample short.  Accumulated over time, this
causes audio/video clock drift.

**Fix:** Changed to ceiling division:
```cpp
int64_t samplesPerFrame = (sampleRate + fps - 1) / fps;
```
This rounds up so the buffer always holds at least one full frame's worth of
samples.

---

### B4 — `memkv::setAll` incremented epoch even when nothing was written
**File:** `source/cpp/memkv.cpp`

**Problem:** When the input map was empty or contained only unknown names,
`setAll` still incremented `*pEpoch` and called `pCond->notify_all()`.  Every
blocked `waitForChange` caller woke up, scanned all slots, found nothing new,
and went back to sleep — spurious wakeups under a lock.

**Fix:** Epoch is incremented and `notify_all` is called only when at least one
slot was actually written:
```cpp
if (written > 0)
{
    *pEpoch = newEpoch;
    pCond->notify_all();
}
```

---

### B5 — `memvid_reader::readNext` and `memaud_reader::readNext` returned an in-flight buffer on overrun
**File:** `include/libmembus/helpers.h`

**Problem:** On overrun, `resync()` set `m_pos = getPtr(0)` (the writer's
current slot) and immediately returned `getBuf(m_pos)` — pointing at the slot
the writer was actively filling.  The returned view contained unspecified,
potentially torn data with no clear signal to the caller that it must not be
used.

**Fix:** Added a documentation comment on both `readNext` overloads making the
contract explicit:
> On overrun `*pOverrun` is set true and the returned view must not be used (it
> points at the slot the writer is actively filling).  Call `wait()` and
> `readNext()` again to receive the next complete frame/buffer.

A more invasive fix (returning a null/sentinel view) would require an API change
and is deferred; the comment ensures callers know not to dereference the view.

---

### B6 — `memkv::getAll` could livelock under sustained write pressure
**File:** `source/cpp/memkv.cpp`

**Problem:** The epoch-stability retry loop had no iteration cap.  Under a
continuous stream of `setAll()` calls the reader could spin indefinitely.

**Fix:** Added a cap of 100 attempts; if the epoch keeps changing the function
returns a best-effort snapshot and exits:
```cpp
for (int attempt = 0; attempt < 100; ++attempt)
{
    …
    if (e1 == e2) return result;
}
return result; // best-effort after 100 retries
```

---

## Security Issues

### S1 — TOCTOU: shared-memory header fields trusted post-validation in `memvid` and `memaud`
**Files:** `source/cpp/memvid.cpp`, `source/cpp/memaud.cpp`

**Problem:** `getBuf()` and `fill()` re-read `*pBufs`, `*pBlockSz`, `*pWidth`,
`*pHeight`, and `*pScanWidth` directly from shared memory on every call.  Any
peer process with write access to the segment could modify these fields between
the layout-validation performed at `open()` / `open_existing()` and the pointer
arithmetic performed in these accessors.  An attacker could set `*pBlockSz` to
a very large value, causing `idx * blockSz` to compute an out-of-bounds address,
or set `*pBufs = 1` to confine all accesses to slot 0.

**Fix:** Both functions now snapshot all header values into local variables at
the top of the function and validate the computed slot offset against
`m_mem.size()` before dereferencing:
```cpp
int64_t bufs    = *(int64_t*)(p + hv_bufs);   // snapshot
int64_t blockSz = *(int64_t*)(p + hv_blocksz);
…
int64_t dataStart = hv_last + idx * blockSz + fv_last;
int64_t dataSize  = sw * h;
if (slotStart < hv_last || dataStart < slotStart
    || dataSize < 0 || dataStart + dataSize > mapped)
    throw std::exception();  // or return false for fill()
```
Even if a malicious process modifies header fields after the snapshot, the local
copies are used for all arithmetic and the bounds check guards the pointer
dereference.

---

### S2 — Reader handles opened with `read_write` access (violates least-privilege)
**Files:** `source/cpp/memmap.cpp`, `source/cpp/memvid.cpp`, `source/cpp/memaud.cpp`

**Problem:** `memmap::open` always mapped shared memory with `read_write` access.
A "reader" process therefore had full write access to the ring-buffer header and
all frame data, allowing it to corrupt the writer's state (write pointer,
sequence counters, frame payloads).

**Fix:** Added a `bReadOnly` parameter to `memmap::open` (default `false`).
When `true`, the share is opened with `read_only` access mode and the mapping is
also created read-only.

`memvid::open_existing` and `memaud::open_existing` — the paths used exclusively
by readers — now pass `bReadOnly=true`:
```cpp
m_mem.open(sName, 0, false, false, /*bReadOnly=*/true)
```

`memmsg` and `memcmd` readers cannot use `read_only` because they acquire an
`interprocess_mutex` stored inside the shared region, which requires write
access.  Those remain `read_write` and the interprocess-mutex design is noted as
an architectural limitation.

---

### S3 — `std::cout` diagnostic output from library internals
**Files:** `source/cpp/memmsg.cpp`, `source/cpp/memcmd.cpp`, `source/cpp/memkv.cpp`

**Problem:** Fourteen `std::cout` call sites printed diagnostic messages
directly to the application's standard output.  Problems:
- Applications cannot suppress or redirect library diagnostics.
- Output could interleave with application output in multi-threaded programs.
- Error details (buffer sizes, internal offsets, counter values) were visible to
  any process reading the application's stdout/logs.

**Fix:** All fourteen `std::cout` calls were removed.  All failure paths already
set an `errc` value via `set_last_error()`; callers can retrieve the reason with
`mmb::last_error()` / `mmb::last_error_message()`.

---

### S4 — Instance-style call to static `shared_memory_object::remove`
**File:** `source/cpp/memmap.cpp` (`close()`)

**Problem:** The original code called `_d->m_mem->remove(m_sName.c_str())` — an
instance-method-style invocation of a static function.  While technically valid
C++, it relies on the compiler ignoring `this`, is misleading, and could mask
bugs if the pointer is null on some implementations.

**Fix:** Changed to the canonical static call:
```cpp
shared_memory_object::remove(m_sName.c_str());
```

---

### S5 — No access control beyond OS file permissions (architectural limitation)
Named POSIX shared memory objects (`/dev/shm/<name>`) are accessible to any
process whose user/group has filesystem read/write permission on the segment.
There is no per-handle authentication or capability check in the library.

**Status:** Not fixed in this changeset — implementing access control would
require a significant API redesign (e.g., per-segment permission bits passed at
creation, or a broker-based model).  The library is designed for cooperative
IPC between trusted processes in the same security domain.  Users who require
stricter isolation should set restrictive umask values or use OS-level
namespaces/containers.

---

## Summary

| ID | Severity | Category | Status |
|----|----------|----------|--------|
| B1 | Medium   | Bug — dead parameter | Fixed |
| B2 | High     | Bug — unaligned access / UB | Fixed |
| B3 | Low      | Bug — audio clock drift | Fixed |
| B4 | Low      | Bug — spurious wakeups | Fixed |
| B5 | Medium   | Bug — stale view on overrun | Documented |
| B6 | Low      | Bug — potential livelock | Fixed |
| S1 | High     | Security — TOCTOU OOB access | Fixed |
| S2 | High     | Security — over-privileged reader handles | Fixed (vid/aud) |
| S3 | Medium   | Security — library stdout leaks | Fixed |
| S4 | Low      | Security — misleading static-as-instance call | Fixed |
| S5 | Medium   | Security — no access control | Documented (arch limitation) |
