#pragma once

namespace LIBMEMBUS_NS
{

class memmsg_writer
{
public:
    bool open(const std::string &name, int64_t size, bool bNew = true)
    {
        if (bNew)
            memmsg::remove(name);
        return m_q.open(name, size, true, true);
    }

    bool write(const std::string &msg) { return m_q.write(msg); }
    void close() { m_q.close(); }
    int64_t getSessionId() { return m_q.getSessionId(); }
    memmsg &raw() { return m_q; }

private:
    memmsg m_q;
};

class memmsg_reader
{
public:
    bool open(const std::string &name, int64_t size)
    { return m_q.open(name, size, false, false); }

    std::string read(uint64_t wait_ms, bool *pOverrun = nullptr)
    { return m_q.read(wait_ms, pOverrun); }

    void close() { m_q.close(); }
    int64_t getSessionId() { return m_q.getSessionId(); }
    memmsg &raw() { return m_q; }

private:
    memmsg m_q;
};

class memcmd_sender
{
public:
    bool open(const std::string &name, int64_t size, bool bCreate = false)
    { return m_cmd.open(name, size, false, bCreate); }

    bool write(const std::string &cmd) { return m_cmd.write(cmd); }
    void close() { m_cmd.close(); }
    int64_t getSessionId() { return m_cmd.getSessionId(); }
    memcmd &raw() { return m_cmd; }

private:
    memcmd m_cmd;
};

class memcmd_receiver
{
public:
    bool open(const std::string &name, int64_t size, bool bCreate = true, bool bNew = true)
    {
        if (bCreate && bNew)
            memcmd::remove(name);
        return m_cmd.open(name, size, true, bCreate);
    }

    std::string read(uint64_t wait_ms, bool *pOverrun = nullptr)
    { return m_cmd.read(wait_ms, pOverrun); }

    int64_t readerCount() { return m_cmd.readerCount(); }
    void close() { m_cmd.close(); }
    int64_t getSessionId() { return m_cmd.getSessionId(); }
    memcmd &raw() { return m_cmd; }

private:
    memcmd m_cmd;
};

class memvid_writer
{
public:
    bool open(const std::string &name, int64_t w, int64_t h, video_format fmt, int64_t fps, int64_t bufs)
    { return m_vid.open(name, true, w, h, fmt, fps, bufs); }

    bool fill(int64_t idx, int col) { return m_vid.fill(idx, col); }
    int64_t next(int64_t inc = 1) { return m_vid.next(inc); }
    int64_t getPtr(int64_t offset = 0) { return m_vid.getPtr(offset); }
    void close() { m_vid.close(); }
    memvid &raw() { return m_vid; }

private:
    memvid m_vid;
};

class memvid_reader
{
public:
    bool open(const std::string &name)
    {
        if (!m_vid.open_existing(name))
            return false;
        resync();
        return true;
    }

    bool wait(uint64_t wait_ms) { return m_vid.waitForFrame(wait_ms, m_lastSeq); }

    memvid::vidview readNext(bool *pOverrun = nullptr)
    {
        if (pOverrun) *pOverrun = false;

        int64_t seq = m_vid.getSeq();
        if (seq - m_lastSeq >= m_vid.getBufs())
        {
            resync();
            if (pOverrun) *pOverrun = true;
            set_last_error(errc::overrun);
            return m_vid.getBuf(m_pos);
        }

        memvid::vidview view = m_vid.getBuf(m_pos);
        m_lastSeq = m_vid.getFrameSeq(m_pos);
        m_pos = (m_pos + 1) % m_vid.getBufs();
        set_last_error(errc::ok);
        return view;
    }

    void resync()
    {
        m_lastSeq = m_vid.getSeq();
        m_pos = m_vid.getPtr(0);
    }

    void close() { m_vid.close(); }
    memvid &raw() { return m_vid; }

private:
    memvid m_vid;
    int64_t m_lastSeq = 0;
    int64_t m_pos = 0;
};

class memaud_writer
{
public:
    bool open(const std::string &name, int64_t ch, audio_format fmt, int64_t sampleRate,
              int64_t fps, int64_t bufs, bool bNew = true)
    { return m_aud.open(name, true, ch, fmt, sampleRate, fps, bufs); }

    bool fill(int64_t idx, int col) { return m_aud.fill(idx, col); }
    int64_t next(int64_t inc = 1) { return m_aud.next(inc); }
    int64_t getPtr(int64_t offset = 0) { return m_aud.getPtr(offset); }
    void close() { m_aud.close(); }
    memaud &raw() { return m_aud; }

private:
    memaud m_aud;
};

class memaud_reader
{
public:
    bool open(const std::string &name)
    {
        if (!m_aud.open_existing(name))
            return false;
        resync();
        return true;
    }

    bool wait(uint64_t wait_ms) { return m_aud.waitForFrame(wait_ms, m_lastSeq); }

    memaud::audview readNext(bool *pOverrun = nullptr)
    {
        if (pOverrun) *pOverrun = false;

        int64_t seq = m_aud.getSeq();
        if (seq - m_lastSeq >= m_aud.getBufs())
        {
            resync();
            if (pOverrun) *pOverrun = true;
            set_last_error(errc::overrun);
            return m_aud.getBuf(m_pos);
        }

        memaud::audview view = m_aud.getBuf(m_pos);
        m_lastSeq = m_aud.getFrameSeq(m_pos);
        m_pos = (m_pos + 1) % m_aud.getBufs();
        set_last_error(errc::ok);
        return view;
    }

    void resync()
    {
        m_lastSeq = m_aud.getSeq();
        m_pos = m_aud.getPtr(0);
    }

    void close() { m_aud.close(); }
    memaud &raw() { return m_aud; }

private:
    memaud m_aud;
    int64_t m_lastSeq = 0;
    int64_t m_pos = 0;
};

}; // end namespace
