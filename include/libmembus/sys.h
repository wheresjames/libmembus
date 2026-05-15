#pragma once

namespace LIBMEMBUS_NS
{

/** Error codes returned by the thread-local diagnostic helpers.
 *
 *  Every API that can fail sets the thread-local last-error value.  Read it
 *  with last_error() / last_error_message() after any call that returned false
 *  or an empty string.
 */
enum class errc
{
    ok = 0,            ///< Operation succeeded.
    invalid_argument,  ///< A caller-supplied argument was out of range or zero.
    open_failed,       ///< The share could not be opened (e.g. not yet created).
    create_failed,     ///< The share could not be created.
    map_failed,        ///< Mapping the shared region into the process address space failed.
    size_mismatch,     ///< The attaching process requested a different logical size than the share was created with.
    invalid_layout,    ///< Header fields are internally inconsistent or the share is too small for the described layout.
    not_open,          ///< The handle is not currently open.
    access_denied,     ///< The operation requires write access but the handle was opened read-only or without write permission.
    message_too_large, ///< The message payload exceeds the ring buffer's logical capacity.
    lock_timeout,      ///< The interprocess mutex could not be acquired within the 5-second crash-recovery deadline.
    timeout,           ///< A blocking read or wait call returned because its deadline expired with no data available.
    overrun            ///< The writer lapped this reader; the read position has been resynced to the current write position.
};

/** Set the thread-local last-error code.
 *  @param e  Error code to store.
 */
void set_last_error(errc e);

/** Return the thread-local last-error code set by the most recent API call on this thread.
 *  @returns The most recent errc value; errc::ok if no error has occurred.
 */
errc last_error();

/** Return a human-readable description for the given error code.
 *  @param e  Error code to describe.
 *  @returns Pointer to a static null-terminated string; never null.
 */
const char *last_error_message(errc e);

/** Return a human-readable description of the thread-local last-error code.
 *  Equivalent to last_error_message(last_error()).
 *  @returns Pointer to a static null-terminated string; never null.
 */
const char *last_error_message();

/** Install a Ctrl-C signal handler that increments a caller-supplied counter.
 *
 *  The counter is incremented each time Ctrl-C is pressed.  If the count
 *  exceeds 3 the process exits immediately via _exit(1) (POSIX) or exit(1)
 *  (Windows).  Typical use:
 *  @code
 *      static volatile int ctrl_c_count = 0;
 *      mmb::install_ctrl_c_handler(&ctrl_c_count);
 *      while (!ctrl_c_count) { ... }
 *  @endcode
 *  @param [out] fCount  Pointer to a counter that is incremented on each press.
 *                       Must remain valid for the lifetime of the process.
 */
void install_ctrl_c_handler(volatile int *fCount);

} // end namespace
