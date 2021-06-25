
#pragma once

#include <napi.h>

/** NAPI macros

//===================================================================
Example: Exporting a function

std::string sayHello(const std::string &name)
{
    return std::string("Hello, ") + name;
}

NAPI_BEGIN_EXPORTS(MyModule)
    NAPI_EXPORT_FUNCTION(sayHello);
NAPI_END_EXPORTS()


//===================================================================
// Exporting a class

class CTestClass
{
public:
    CTestClass() : m_x(0) {};
    CTestClass(int x) : m_x(x) {};
    void setX(int x) { m_x = x; }
    int getX() { return m_x; }
private:
    int m_x;
};

NAPI_DEFINE_CLASS(CTestClass)
    NAPI_CONSTRUCTOR_1(CTestClass)
    NAPI_END_CONSTRUCTORS(CTestClass)
    NAPI_MEMBER_FUNCTION(CTestClass, setX)
    NAPI_MEMBER_FUNCTION(CTestClass, getX)
NAPI_DEFINE_CLASS_END()


NAPI_BEGIN_EXPORTS(MyModule)
    NAPI_EXPORT_CLASS(CTestClass);
NAPI_END_EXPORTS()

*/


/// @cond DOXYGEN_EXCLUDE

namespace LIBBLANK_NS
{

//=================================================================================================
// Function wrapper

class cvtP
{
public:
    cvtP(const Napi::CallbackInfo& info, const int i) : m_info(info), m_i(i) {}
    operator int() { return m_info[m_i].As<Napi::Number>().Int64Value(); }
    operator unsigned int() { return m_info[m_i].As<Napi::Number>().Int64Value(); }
    operator long() { return m_info[m_i].As<Napi::Number>().Int64Value(); }
    operator unsigned long() { return m_info[m_i].As<Napi::Number>().Int64Value(); }
    operator long long() { return m_info[m_i].As<Napi::Number>().Int64Value(); }
    operator unsigned long long() { return m_info[m_i].As<Napi::Number>().Int64Value(); }
    operator float() { return m_info[m_i].As<Napi::Number>().FloatValue(); }
    operator double() { return m_info[m_i].As<Napi::Number>().DoubleValue(); }
    operator std::string() { return m_info[m_i].As<Napi::String>().Utf8Value(); }

private:
    const Napi::CallbackInfo        &m_info;
    const int                       m_i;
};

template<class R>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)())
    {   return f(); }
template<class R, class P1>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)(P1))
    {   return f(cvtP(info, 0)); }
template<class R, class P1, class P2>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)(P1, P2))
    {   return f(cvtP(info, 0), cvtP(info, 1)); }
template<class R, class P1, class P2, class P3>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)(P1, P2, P3))
    {   return f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2)); }
template<class R, class P1, class P2, class P3, class P4>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)(P1, P2, P3, P4))
    {   return f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3)); }
template<class R, class P1, class P2, class P3, class P4, class P5>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)(P1, P2, P3, P4, P5))
    {   return f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3), cvtP(info, 4)); }
template<class R, class P1, class P2, class P3, class P4, class P5, class P6>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)(P1, P2, P3, P4, P5, P6))
    {   return f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3), cvtP(info, 4), cvtP(info, 5)); }
template<class R, class P1, class P2, class P3, class P4, class P5, class P6, class P7>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)(P1, P2, P3, P4, P5, P6, P7))
    {   return f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3), cvtP(info, 4), cvtP(info, 5), cvtP(info, 6)); }
template<class R, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8>
    R callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, R(*f)(P1, P2, P3, P4, P5, P6, P7, P8))
    {   return f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3), cvtP(info, 4), cvtP(info, 5), cvtP(info, 6), cvtP(info, 7)); }

int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)())
{   f(); return 0; }
template<class P1>
    int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)(P1))
    {   f(cvtP(info, 0)); return 0; }
template<class P1, class P2>
    int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)(P1, P2))
    {   f(cvtP(info, 0), cvtP(info, 1)); return 0; }
template<class P1, class P2, class P3>
    int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)(P1, P2, P3))
    {   f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2)); return 0; }
template<class P1, class P2, class P3, class P4>
    int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)(P1, P2, P3, P4))
    {   f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3)); return 0; }
template<class P1, class P2, class P3, class P4, class P5>
    int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)(P1, P2, P3, P4, P5))
    {   f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3), cvtP(info, 4)); return 0; }
template<class P1, class P2, class P3, class P4, class P5, class P6>
    int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)(P1, P2, P3, P4, P5, P6))
    {   f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3), cvtP(info, 4), cvtP(info, 5)); return 0; }
template<class P1, class P2, class P3, class P4, class P5, class P6, class P7>
    int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)(P1, P2, P3, P4, P5, P6, P7))
    {   f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3), cvtP(info, 4), cvtP(info, 5), cvtP(info, 6)); return 0; }
template<class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8>
    int callFunction(const Napi::Env &env, const Napi::CallbackInfo &info, void(*f)(P1, P2, P3, P4, P5, P6, P7, P8))
    {   f(cvtP(info, 0), cvtP(info, 1), cvtP(info, 2), cvtP(info, 3), cvtP(info, 4), cvtP(info, 5), cvtP(info, 6), cvtP(info, 7)); return 0; }


template<class C, class R>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)())
    {   return (c->*f)(); }
template<class C, class R, class P1>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)(P1))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        return (c->*f)(cvtP(_info, 0)); }
template<class C, class R, class P1, class P2>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)(P1, P2))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        return (c->*f)(cvtP(_info, 0), cvtP(_info, 1)); }
template<class C, class R, class P1, class P2, class P3>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)(P1, P2, P3))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        return (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2)); }
template<class C, class R, class P1, class P2, class P3, class P4>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)(P1, P2, P3, P4))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        return (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3)); }
template<class C, class R, class P1, class P2, class P3, class P4, class P5>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)(P1, P2, P3, P4, P5))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        return (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3), cvtP(_info, 4)); }
template<class C, class R, class P1, class P2, class P3, class P4, class P5, class P6>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)(P1, P2, P3, P4, P5, P6))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        return (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3), cvtP(_info, 4), cvtP(_info, 5)); }
template<class C, class R, class P1, class P2, class P3, class P4, class P5, class P6, class P7>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)(P1, P2, P3, P4, P5, P6, P7))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        return (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3), cvtP(_info, 4), cvtP(_info, 5), cvtP(_info, 6)); }
template<class C, class R, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8>
    R callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, R(C::*f)(P1, P2, P3, P4, P5, P6, P7, P8))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        return (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3), cvtP(_info, 4), cvtP(_info, 5), cvtP(_info, 6), cvtP(_info, 7)); }

template<class C>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)())
    {   (c->*f)(); return 0; }
template<class C, class P1>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)(P1))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        (c->*f)(cvtP(_info, 0)); return 0; }
template<class C, class P1, class P2>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)(P1, P2))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        (c->*f)(cvtP(_info, 0), cvtP(_info, 1)); return 0; }
template<class C, class P1, class P2, class P3>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)(P1, P2, P3))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2)); return 0; }
template<class C, class P1, class P2, class P3, class P4>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)(P1, P2, P3, P4))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3)); return 0; }
template<class C, class P1, class P2, class P3, class P4, class P5>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)(P1, P2, P3, P4, P5))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3), cvtP(_info, 4)); return 0; }
template<class C, class P1, class P2, class P3, class P4, class P5, class P6>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)(P1, P2, P3, P4, P5, P6))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3), cvtP(_info, 4), cvtP(_info, 5)); return 0; }
template<class C, class P1, class P2, class P3, class P4, class P5, class P6, class P7>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)(P1, P2, P3, P4, P5, P6, P7))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3), cvtP(_info, 4), cvtP(_info, 5), cvtP(_info, 6)); return 0; }
template<class C, class P1, class P2, class P3, class P4, class P5, class P6, class P7, class P8>
    int callMemberFunction(const napi_env &env, const napi_callback_info &info, C *c, void (C::*f)(P1, P2, P3, P4, P5, P6, P7, P8))
    {   Napi::Env _env(env); Napi::CallbackInfo _info(_env, info);
        (c->*f)(cvtP(_info, 0), cvtP(_info, 1), cvtP(_info, 2), cvtP(_info, 3), cvtP(_info, 4), cvtP(_info, 5), cvtP(_info, 6), cvtP(_info, 7)); return 0; }


static Napi::String cvtV(const Napi::Env &env, const std::string &v)
{    return Napi::String::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, int v)
{    return Napi::Number::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, unsigned int v)
{    return Napi::Number::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, long v)
{    return Napi::Number::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, unsigned long v)
{    return Napi::Number::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, long long v)
{    return Napi::Number::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, unsigned long long v)
{    return Napi::Number::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, float v)
{    return Napi::Number::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, double v)
{    return Napi::Number::New(env, v); }
static Napi::Number cvtV(const Napi::Env &env, long double v)
{    return Napi::Number::New(env, v); }


#define NAPI_BEGIN_EXPORTS(n) static const char *_napi_module_name = #n; \
                              static Napi::Object _napi_module_init(Napi::Env env, Napi::Object exports) \
                              { Napi::HandleScope scope(env);
#define NAPI_END_EXPORTS() return exports; } NODE_API_MODULE(_napi_module_name, _napi_module_init)

#define NAPI_EXPORT_FUNCTION(f) \
                                exports.Set(Napi::String::New(env, #f), \
                                       Napi::Function::New(env, \
                                            [](const Napi::CallbackInfo& info) \
                                            {   Napi::Env env = info.Env(); \
                                                return lbk::cvtV(env, lbk::callFunction(env, info, f)); \
                                            }) \
                                       );

#define NAPI_EXPORT_FUNCTION_AS(f, n) \
                                exports.Set(Napi::String::New(env, n), \
                                       Napi::Function::New(env, \
                                            [](const Napi::CallbackInfo& info) \
                                            {   Napi::Env env = info.Env(); \
                                                return lbk::cvtV(env, lbk::callFunction(env, info, f)); \
                                            }) \
                                       );


//=================================================================================================
// Class wrapper

#define NAPI_CONSTRUCTOR_1(c) if (1 == info.Length()) m_c = c(lbk::cvtP(info, 0));
#define NAPI_CONSTRUCTOR_2(c) if (2 == info.Length()) m_c = c(lbk::cvtP(info, 0), lbk::cvtP(info, 1));
#define NAPI_CONSTRUCTOR_3(c) if (3 == info.Length()) \
    m_c = c(lbk::cvtP(info, 0), lbk::cvtP(info, 1), lbk::cvtP(info, 2));
#define NAPI_CONSTRUCTOR_4(c) if (4 == info.Length()) \
    m_c = c(lbk::cvtP(info, 0), lbk::cvtP(info, 1), lbk::cvtP(info, 2), lbk::cvtP(info, 3));
#define NAPI_CONSTRUCTOR_5(c) if (5 == info.Length()) \
    m_c = c(lbk::cvtP(info, 0), lbk::cvtP(info, 1), lbk::cvtP(info, 2), lbk::cvtP(info, 3), \
            lbk::cvtP(info, 4));
#define NAPI_CONSTRUCTOR_6(c) if (6 == info.Length()) \
    m_c = c(lbk::cvtP(info, 0), lbk::cvtP(info, 1), lbk::cvtP(info, 2), lbk::cvtP(info, 3), \
            lbk::cvtP(info, 4), lbk::cvtP(info, 5));
#define NAPI_CONSTRUCTOR_7(c) if (7 == info.Length()) \
    m_c = c(lbk::cvtP(info, 0), lbk::cvtP(info, 1), lbk::cvtP(info, 2), lbk::cvtP(info, 3), \
            lbk::cvtP(info, 4), lbk::cvtP(info, 5), lbk::cvtP(info, 6));
#define NAPI_CONSTRUCTOR_8(c) if (8 == info.Length()) \
    m_c = c(lbk::cvtP(info, 0), lbk::cvtP(info, 1), lbk::cvtP(info, 2), lbk::cvtP(info, 3), \
            lbk::cvtP(info, 4), lbk::cvtP(info, 5), lbk::cvtP(info, 6), lbk::cvtP(info, 7));

#define NAPI_DEFINE_CLASS(c) \
    static Napi::FunctionReference ClassWrapper_constructor_##c; \
    class ClassWrapper_##c : public Napi::ObjectWrap<ClassWrapper_##c> \
    { private: c m_c; \
      public: \
        static ClassWrapper_##c* getClassPtr(napi_env env, napi_callback_info info) \
        {   size_t _argc = 6; napi_value _argv[6]; napi_value _this; void* _data = 0; \
            napi_status status = napi_get_cb_info(env, info, &_argc, _argv, &_this, &_data); \
            if ((status) != napi_ok) throw Napi::Error::New(env); \
            if (!_this) throw Napi::Error::New(env); \
            ClassWrapper_##c *pf = Unwrap(Napi::Object(env, _this).As<Napi::Object>()); \
            if (!pf) throw Napi::Error::New(env); \
            return pf; \
        } \
        static napi_property_descriptor getDesc(const char *pFn, napi_value(*pF)(napi_env env, napi_callback_info info)) \
        {   napi_property_descriptor desc = napi_property_descriptor(); \
            desc.utf8name = pFn; desc.method = pF; desc.data = 0; desc.attributes = napi_default; \
            return desc; \
        } \
        ~ClassWrapper_##c() {} \
        ClassWrapper_##c(const Napi::CallbackInfo& info) : Napi::ObjectWrap<ClassWrapper_##c>(info) \
        { \

#define NAPI_END_CONSTRUCTORS(c) \
        } \
        static void Init(const Napi::Env &env, Napi::Object &exports) \
        {   const char *pClassName = #c; \
            Napi::FunctionReference *pcr = &ClassWrapper_constructor_##c; \
            Napi::Function func = ClassWrapper_##c::DefineClass(env, #c, \
            {

#define NAPI_MEMBER_FUNCTION(c, f) \
            getDesc(#f, \
                    [](napi_env env, napi_callback_info info) -> napi_value \
                    {   return lbk::cvtV(env, lbk::callMemberFunction(env, info, &getClassPtr(env, info)->m_c, &c::f)); \
                    }),

#define NAPI_MEMBER_FUNCTION_AS(c, f, n) \
            getDesc(n, \
                    [](napi_env env, napi_callback_info info) -> napi_value \
                    {   return lbk::cvtV(env, lbk::callMemberFunction(env, info, &getClassPtr(env, info)->m_c, &c::f)); \
                    }),

#define NAPI_DEFINE_CLASS_END() \
            }); \
            *pcr = Napi::Persistent(func); \
            pcr->SuppressDestruct(); \
            exports.Set(pClassName, func); \
        } \
    };

#define NAPI_EXPORT_CLASS(c) ClassWrapper_##c::Init(env, exports);

} // end namespace

/// @endcond DOXYGEN_EXCLUDE
