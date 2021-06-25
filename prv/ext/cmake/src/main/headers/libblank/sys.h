
#pragma once

namespace LIBBLANK_NS
{

    /** Install ctrl-c handler
        @param [out]    - Flag is incremented each time ctrl-c is pressed
    */
    void install_ctrl_c_handler(volatile int *fCount);

} // end namespace
