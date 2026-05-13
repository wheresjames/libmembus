
#pragma once

namespace LIBMEMBUS_NS
{

    enum class errc
    {
        ok = 0,
        invalid_argument,
        open_failed,
        create_failed,
        map_failed,
        size_mismatch,
        invalid_layout,
        not_open,
        access_denied,
        message_too_large,
        lock_timeout,
        timeout,
        overrun
    };

    void set_last_error(errc e);
    errc last_error();
    const char *last_error_message(errc e);
    const char *last_error_message();

    /** Install ctrl-c handler
        @param [out]    - Flag is incremented each time ctrl-c is pressed
    */
    void install_ctrl_c_handler(volatile int *fCount);

} // end namespace
