
#pragma once

namespace LIBBLANK_NS
{

/** This is an example class

    This class isn't good for anything, it's just an example.

    @note You should remove this class and add classes that are usefull.

*/
class CBlank
{

public:

    /// Default constructor
    CBlank() : m_nothing(0) {}

    /// Default destructor
    virtual ~CBlank() { m_nothing = 0; }

    /** Set nothing
        @param [in] x   - Pointless input value

        @returns The same pointless value you sent in.

        <b>Example:</b>
        @code{.cpp}

            // Declare a useless class instance
            CBlank b;

            // Do something pointless
            b.set_nothing(99);

        @endcode

    */
    int set_nothing(int x) { return m_nothing = x; }

    /** Get nothing
        @returns If a pointless value has been set with set_nothing(),
                 this function will return that value.
    */
    int get_nothing() { return m_nothing; }

protected:

    /// Nothing, holds a pointeless value for no good reason.
    int                 m_nothing;

};

/** This class extends CBlank to make it even more useless

    They say sometimes less is better, and this class proves that point.
    It extends CBlank to somehow make it even more pointless.

    <b>Waste of time</b>
    @code{.cpp}

        CBlanker b;

        b.set_nothing(99);

        b.do_nothing();

    @endcode

*/
class CBlanker: public CBlank
{
public:

    /// Constructor that is completely useless
    CBlanker() : CBlank() {}

    /// This destructor does nothing and is pointless
    virtual ~CBlanker() {}

    /** This function doesn't actually "show" anything.

        Call this function anytime you like, it doesn't do anything.

        @note This functions name is a oxymoron.
    */
    int show_nothing() { return m_nothing; }


    /** This function does nothing

        @note Finally, a function with an honest name.
    */
    void do_nothing() {}
};

} // end namespace
