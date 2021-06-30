
#include "libmembus.h"


//=================================================================================================
// Test Functions

/// Test function, takes two parameters and returns a string
std::string sayHello(int x, const std::string &y)
{
    return std::string("Hello, ") + std::to_string(x) + ", " + y;
}

/// Test function, takes one parameter and returns it
double showNum(double x)
{
    return x;
}

/// This test function has a void return type
void noReturn(int x)
{
}


//=================================================================================================
// Test Class

/** Test class

    This class is for testing the node js import.

*/
class CTestClass
{
public:

    /// Default constructor
    CTestClass() : m_x(0) {};

    /// One parameter constructor
    CTestClass(int x) : m_x(x) {};

    /// Set x to value
    /// @param [in] x   - Value to set
    void setX(int x) { m_x = x; }

    /// Returns value of x
    int getX() { return m_x; }

private:

    int m_x;
};


//=================================================================================================

// Create a wrapper for the CTestClass
NAPI_DEFINE_CLASS(CTestClass)

    NAPI_CONSTRUCTOR_1(CTestClass)
    NAPI_END_CONSTRUCTORS(CTestClass)

    NAPI_MEMBER_FUNCTION(CTestClass, setX)
    NAPI_MEMBER_FUNCTION(CTestClass, getX)

NAPI_DEFINE_CLASS_END()


NAPI_BEGIN_EXPORTS(libmembus)

    // Export CTestClass
    NAPI_EXPORT_CLASS(CTestClass);

    // Export simple module functions
    NAPI_EXPORT_FUNCTION(sayHello);
    NAPI_EXPORT_FUNCTION(showNum);
    NAPI_EXPORT_FUNCTION(noReturn);

NAPI_END_EXPORTS()

