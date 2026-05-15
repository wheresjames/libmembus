
#include "libmembus-internal.h"

namespace LIBMEMBUS_NS
{

int select(uint64_t wait_ms, const std::vector<std::function<bool()>> &conditions)
{
    if (conditions.empty())
        return -1;

    auto deadline = std::chrono::steady_clock::now()
                    + std::chrono::milliseconds(wait_ms);

    do
    {
        for (int i = 0; i < (int)conditions.size(); ++i)
            if (conditions[i]()) return i;

        if (wait_ms == 0)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (std::chrono::steady_clock::now() < deadline);

    return -1;
}

int select(uint64_t wait_ms,
           std::initializer_list<std::function<bool()>> conditions)
{
    return select(wait_ms,
                  std::vector<std::function<bool()>>(conditions.begin(),
                                                     conditions.end()));
}

} // end namespace
