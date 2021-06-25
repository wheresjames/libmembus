#pragma once


namespace LIBMEMBUS_NS
{

/** Memory mapped message buffer

    <b>Example:</b>
    @code

        mmb::memmsg tx, rx;

        if (!tx.open("/mymsg", 128, true, true))
            return -1;

        if (!rx.open("/mymsg", 128, false, false))
            return -1;

        for(int i = 0; i < 1000; i++)
        {
            std::string msg = std::string("Message ") + std::to_string(i);

            if (!tx.write(msg))
                return -1;

            std::string rmsg = rx.read(0);
            if (rmsg != msg)
                return -1;
        }

    @endcode

*/
class memmsg
{

public:

    /// Constructor
    memmsg() : m_bWrite(false), m_nRead(0) {};

    /// Destructor
    virtual ~memmsg() { close(); }

    /** Creates / attaches to an image ring buffer in memory
        @param [in] sName   - Name of memory share to open
        @param [in] size    - Size of the memory share
        @param [in] bWrite  - Non-zero to open for writing, otherwise opened for read-only
        @param [in] bCreate - Non-zero if the share should be created if it doesn't exist

        @returns Non-zero if success.
    */
    bool open(const std::string &sName, int64_t size, bool bWrite, bool bCreate);

    /** Close image share

    */
    void close();

    /** Write a message to the buffer
        @param [in] sMsg    - Message to write into share

        @returns Non-zero if message was written.

        @note This function will fail if there is not enough room to write the message.
    */
    bool write(const std::string &sMsg);

    /** Read a message from the buffer
        @param [in] wait    - Maximum number of milliseconds to wait for a message to appear.

        @returns Message that was read or an empty string if function timed out waiting for a message.
    */
    std::string read(uint64_t wait);

    /** Returns true if memory already existed
        @returns Non-zero if memory share already existed.
    */
    bool existing() { return m_mem.existing(); }

private:

    /// The memory map
    memmap      m_mem;

    /// Non-zero if this is the writer
    bool        m_bWrite;

    /// Read pointer
    int64_t     m_nRead;

};

}; // end namespace
