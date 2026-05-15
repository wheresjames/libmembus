#pragma once

#include <chrono>
#include <functional>
#include <initializer_list>
#include <thread>
#include <vector>

namespace LIBMEMBUS_NS
{

/** Wait until any one of the supplied conditions becomes true, or until
 *  @p wait_ms milliseconds elapse.
 *
 *  Each condition is a callable @c bool() that returns @c true when its
 *  associated source has data ready.  Conditions are checked in index order
 *  at ~1 ms intervals (one pass per millisecond); the first condition that
 *  returns @c true wins.
 *
 *  This approach is intentionally simple: it works uniformly for lock-free
 *  rings (memvid, memaud) and mutex-backed queues (memmsg, memcmd) without
 *  requiring a unified wake-up mechanism or changes to the wire format.
 *
 *  **Typical use:**
 *  @code
 *      mmb::memvid_reader  vid;  vid.open("/video");
 *      mmb::memcmd_receiver cmd; cmd.open("/commands", 4096);
 *
 *      int64_t vidSeq = vid.raw().getSeq();
 *
 *      while (running) {
 *          int idx = mmb::select(100, {
 *              [&]{ return vid.raw().getSeq() > vidSeq; },
 *              [&]{ return cmd.raw().poll(); }
 *          });
 *          if (idx == 0) { auto frame = vid.readNext(); ... vidSeq = vid.raw().getSeq() - 1; }
 *          if (idx == 1) { auto c = cmd.read(0); ... }
 *      }
 *  @endcode
 *
 *  @param wait_ms    Maximum milliseconds to wait.  Pass 0 for a single
 *                    non-blocking pass over all conditions.
 *  @param conditions List of callables.  Each must be cheap (non-blocking)
 *                    and must not consume the data it detects; do the actual
 *                    read after select() returns.
 *  @returns Zero-based index of the first ready condition, or -1 on timeout.
 */
int select(uint64_t wait_ms,
           std::initializer_list<std::function<bool()>> conditions);

/** Overload that accepts a pre-built vector of conditions.
 *  Semantics are identical to the initializer-list overload.
 *
 *  @param wait_ms    Maximum milliseconds to wait.
 *  @param conditions Vector of callables.
 *  @returns Zero-based index of the first ready condition, or -1 on timeout.
 */
int select(uint64_t wait_ms,
           const std::vector<std::function<bool()>> &conditions);

} // end namespace
