
#include <pybind11/pybind11.h>

#include "libblank.h"

namespace py = pybind11;
namespace LIBBLANK_NS
{

class pytest
{
public:

    pytest() {}
    virtual ~pytest() {}

    int add(int x, int y) { return x + y; }
};

PYBIND11_MODULE(APPNAMERAW, m)
{
    m.doc() = APPDESC;
    m.attr("__version__") = APPVER;
    m.attr("__build__") = APPBUILD;
    m.def("version", [] () { return APPVER; });
    m.def("build", [] () { return APPBUILD; });

    py::class_<pytest>(m, "pytest")
        .def(py::init<>())
        .def("add", &pytest::add)
        ;
}

}; // end namespace

