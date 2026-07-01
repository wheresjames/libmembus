#include "libmembus.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/utsname.h>
#endif

namespace
{
    using clock_type = std::chrono::steady_clock;

    // ── Throughput result ────────────────────────────────────────────────────

    struct result
    {
        std::string name;
        std::string group;
        std::string payload;
        uint64_t operations = 0;
        uint64_t bytes = 0;
        double seconds = 0.0;

        double ops_per_sec() const { return seconds > 0.0 ? (double)operations / seconds : 0.0; }
        double mb_per_sec()  const { return seconds > 0.0 ? ((double)bytes / (1024.0 * 1024.0)) / seconds : 0.0; }
        double ns_per_op()   const { return operations > 0 ? seconds * 1e9 / (double)operations : 0.0; }
    };

    // ── Latency result ───────────────────────────────────────────────────────

    struct latency_result
    {
        std::string name;
        std::string group;
        std::string payload;
        std::vector<double> samples_us;   // populated by bench functions; sorted before reporting

        double percentile(double p) const
        {
            if (samples_us.empty()) return 0.0;
            size_t idx = (size_t)(p / 100.0 * (double)samples_us.size());
            if (idx >= samples_us.size()) idx = samples_us.size() - 1;
            return samples_us[idx];
        }
        double p50()    const { return percentile(50); }
        double p95()    const { return percentile(95); }
        double p99()    const { return percentile(99); }
        double min_us() const { return samples_us.empty() ? 0.0 : samples_us.front(); }
        double max_us() const { return samples_us.empty() ? 0.0 : samples_us.back(); }
        size_t count()  const { return samples_us.size(); }
    };

    // ── Options ──────────────────────────────────────────────────────────────

    struct options
    {
        int duration_ms = 1000;
        std::string json_path = "bench/results/latest.json";
    };

    // ── Helpers ──────────────────────────────────────────────────────────────

    int64_t now_ns()
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            clock_type::now().time_since_epoch()).count();
    }

    std::string unique_name(const std::string &suffix)
    {
        return "/libmembus_bench_" + std::to_string(now_ns()) + "_" + suffix;
    }

    std::string json_escape(const std::string &s)
    {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s)
        {
            if (c == '\\' || c == '"') { out.push_back('\\'); out.push_back(c); }
            else if (c == '\n') out += "\\n";
            else out.push_back(c);
        }
        return out;
    }

    std::string system_info()
    {
#if defined(__unix__) || defined(__APPLE__)
        struct utsname u {};
        if (uname(&u) == 0)
        {
            std::ostringstream os;
            os << u.sysname << " " << u.release << " " << u.machine;
            return os.str();
        }
#endif
        return "unknown";
    }

    template <typename F>
    result run_for(const std::string &name, const std::string &group,
                   const std::string &payload, int duration_ms, F &&fn)
    {
        result r;
        r.name    = name;
        r.group   = group;
        r.payload = payload;

        auto warm_end = clock_type::now() + std::chrono::milliseconds(std::max(50, duration_ms / 10));
        while (clock_type::now() < warm_end)
            fn();

        auto start = clock_type::now();
        auto end   = start + std::chrono::milliseconds(duration_ms);
        while (clock_type::now() < end)
        {
            auto bytes = fn();
            r.operations++;
            r.bytes += bytes;
        }
        r.seconds = std::chrono::duration<double>(clock_type::now() - start).count();
        return r;
    }

    // ── Throughput benchmarks ─────────────────────────────────────────────────

    result bench_memmap(int duration_ms)
    {
        constexpr size_t payload_size = 64 * 1024;
        std::string payload(payload_size, 'm');
        mmb::memmap map;
        if (!map.open(unique_name("memmap"), payload_size, true, true))
            throw std::runtime_error("memmap open failed");

        return run_for("memmap read/write", "memmap", "64 KiB", duration_ms, [&]() -> uint64_t {
            map.write(payload);
            volatile char c = map.data()[0]; (void)c;
            std::string copy = map.read(payload.size());
            return payload.size() * 2;
        });
    }

    result bench_memmsg(int duration_ms)
    {
        constexpr size_t payload_size = 64;
        std::string payload(payload_size, 'q');
        std::string name = unique_name("memmsg");
        mmb::memmsg tx, rx;
        if (!tx.open(name, 8192, true, true) || !rx.open(name, 8192, false, false))
            throw std::runtime_error("memmsg open failed");

        return run_for("memmsg single writer/reader", "memmsg", "64 B", duration_ms, [&]() -> uint64_t {
            tx.write(payload);
            rx.read(0);
            return payload.size();
        });
    }

    result bench_memcmd(int duration_ms)
    {
        constexpr int writers = 4;
        constexpr size_t payload_size = 64;
        std::string payload(payload_size, 'c');
        std::string name = unique_name("memcmd");
        mmb::memcmd receiver;
        std::vector<mmb::memcmd> senders(writers);
        if (!receiver.open(name, 32768, true, true))
            throw std::runtime_error("memcmd receiver open failed");
        for (auto &s : senders)
            if (!s.open(name, 32768))
                throw std::runtime_error("memcmd sender open failed");

        size_t idx = 0;
        return run_for("memcmd 4 writers/1 reader", "memcmd", "64 B", duration_ms, [&]() -> uint64_t {
            senders[idx].write(payload);
            idx = (idx + 1) % senders.size();
            receiver.read(0);
            return payload.size();
        });
    }

    result bench_memkv_set(int duration_ms)
    {
        std::string name = unique_name("memkv");
        mmb::memkv kv;
        if (!kv.create(name, 8, 16, 64, true))
            throw std::runtime_error("memkv create failed");
        for (int i = 0; i < 8; i++)
            kv.setName(i, "k" + std::to_string(i));

        int v = 0;
        return run_for("memkv setValue", "memkv", "64 B value", duration_ms, [&]() -> uint64_t {
            kv.setValue(v % 8, "value_" + std::to_string(v));
            v++;
            return 64;
        });
    }

    result bench_memkv_get(int duration_ms)
    {
        std::string name = unique_name("memkv_get");
        mmb::memkv kv;
        if (!kv.create(name, 8, 16, 64, true))
            throw std::runtime_error("memkv create failed");
        for (int i = 0; i < 8; i++) { kv.setName(i, "k" + std::to_string(i)); kv.setValue(i, "value"); }

        int v = 0;
        return run_for("memkv getValue", "memkv", "64 B value", duration_ms, [&]() -> uint64_t {
            volatile auto s = kv.getValue(v % 8); (void)s;
            v++;
            return 64;
        });
    }

    result bench_memvid(int duration_ms, int w, int h)
    {
        std::string name = unique_name("memvid");
        mmb::memvid vid;
        if (!vid.open(name, true, w, h, mmb::video_format::rgb24, 30, 4))
            throw std::runtime_error("memvid open failed");

        int64_t frame_bytes = (int64_t)w * h * 3;
        return run_for("memvid publish", "memvid", std::to_string(w) + "x" + std::to_string(h),
                       duration_ms, [&]() -> uint64_t {
            int64_t slot = vid.getPtr(0);
            vid.fill(slot, 0x7f);
            vid.next(1);
            return (uint64_t)frame_bytes;
        });
    }

    result bench_memaud(int duration_ms)
    {
        std::string name = unique_name("memaud");
        mmb::memaud aud;
        if (!aud.open(name, true, 2, mmb::audio_format::s16le, 48000, 100, 4))
            throw std::runtime_error("memaud open failed");

        uint64_t bytes = (uint64_t)aud.getBufSize();
        return run_for("memaud publish", "memaud", "stereo S16LE 48k/100fps",
                       duration_ms, [&]() -> uint64_t {
            int64_t slot = aud.getPtr(0);
            aud.fill(slot, 0);
            aud.next(1);
            return bytes;
        });
    }

    // Publish variable-length records into a mempkt ring.  Unlike memvid/memaud,
    // whose "publish" is a fill() (memset) + next(), mempkt::write() performs a
    // real memcpy of the payload into the arena — so this MiB/s figure includes a
    // genuine copy and is not directly comparable to the fixed-slot rings.
    result bench_mempkt_write(int duration_ms, int record_bytes)
    {
        mmb::mempkt pk;
        int64_t maxrec  = record_bytes;
        int64_t arenasz = (int64_t)record_bytes * 64;   // headroom so writes never self-stall
        if (!pk.open(unique_name("mempkt"), true, 32, arenasz, maxrec))
            throw std::runtime_error("mempkt open failed");

        std::string payload(record_bytes, 'p');
        std::string label = std::to_string(record_bytes) + " B/record (incl. copy)";
        return run_for("mempkt write", "mempkt", label, duration_ms, [&]() -> uint64_t {
            pk.write(payload, mmb::pkt_kind::data);
            return (uint64_t)record_bytes;
        });
    }

    // Consume records from a mempkt ring.  This is the only ring whose read path
    // copies the payload out of shared memory and re-validates it against the
    // write cursor (torn-read safety), so it is the one read-side cost worth
    // measuring on its own.
    result bench_mempkt_read(int duration_ms, int record_bytes)
    {
        std::string name = unique_name("mempkt_read");
        mmb::mempkt_writer w;
        int64_t maxrec  = record_bytes;
        int64_t arenasz = (int64_t)record_bytes * 64;
        if (!w.open(name, 32, arenasz, maxrec))
            throw std::runtime_error("mempkt_read writer open failed");

        mmb::mempkt_reader r;
        if (!r.open(name))
            throw std::runtime_error("mempkt_read reader open failed");

        std::string payload(record_bytes, 'p');
        std::string out, meta;
        mmb::mempkt::recinfo info;
        std::string label = std::to_string(record_bytes) + " B/record (copy-out)";
        return run_for("mempkt read", "mempkt", label, duration_ms, [&]() -> uint64_t {
            // Publish one, then copy it back out so the measured op is a full
            // getRecord() (seqlock + arena copy + write-cursor re-check).
            w.write(payload, mmb::pkt_kind::data);
            bool overrun = false;
            if (!r.readNext(out, meta, info, &overrun) || overrun)
                return 0;
            return (uint64_t)out.size();
        });
    }

    // ── Latency benchmarks ────────────────────────────────────────────────────

    // Measure the time from memvid::next() in a writer thread to mmb::select()
    // returning in the reader (this) thread.  Timestamps are carried in the vpts
    // frame field so no extra shared state is needed.
    //
    // The writer publishes at ~200 fps (one frame per 5 ms).  With select()
    // polling at 1 ms intervals, the detection latency is approximately
    // uniform(0, poll_interval) ≈ uniform(0, 1 ms).
    latency_result bench_select_wakeup(int duration_ms)
    {
        std::string name = unique_name("sel_vid");
        mmb::memvid vid;
        if (!vid.open(name, true, 8, 4, mmb::video_format::gray8, 200, 8))
            throw std::runtime_error("select wakeup: vid open failed");

        std::atomic<bool> done{false};

        std::thread writer([&] {
            while (!done.load(std::memory_order_acquire))
            {
                int64_t slot = vid.getPtr(0);
                vid.fill(slot, 0x42);
                vid.setVpts(slot, now_ns());   // carry write timestamp in the frame
                vid.next(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(5));  // 200 fps
            }
        });

        // Warm up: let the writer run for a moment and discard early samples.
        std::this_thread::sleep_for(std::chrono::milliseconds(std::max(50, duration_ms / 10)));

        latency_result r;
        r.name    = "select() wakeup latency";
        r.group   = "select";
        r.payload = "1 memvid source";

        int64_t last_seq = vid.getSeq();
        auto end = clock_type::now() + std::chrono::milliseconds(duration_ms);

        while (clock_type::now() < end)
        {
            int idx = mmb::select(50, {[&]{ return vid.getSeq() > last_seq; }});
            if (idx == 0)
            {
                int64_t detect_ts = now_ns();
                int64_t pub_slot  = vid.getPtr(-1);          // slot just published
                int64_t write_ts  = vid.getVpts(pub_slot);   // timestamp set by writer
                if (write_ts > 0 && detect_ts > write_ts)
                {
                    double us = (double)(detect_ts - write_ts) / 1000.0;
                    if (us < 50000.0)   // sanity cap at 50 ms
                        r.samples_us.push_back(us);
                }
                last_seq = vid.getSeq();
            }
        }

        done.store(true, std::memory_order_release);
        writer.join();

        std::sort(r.samples_us.begin(), r.samples_us.end());
        return r;
    }

    // Measure single-hop memmsg round-trip latency via an in-process echo thread.
    // Main thread sends a ping; echo thread reads and immediately writes a pong;
    // main thread reads the pong and records the elapsed time.
    //
    // The latency is dominated by two condvar wakeup / mutex acquisition cycles
    // (one per direction).  It closely approximates cross-process latency because
    // the shared-memory path is identical; the only difference in a real deployment
    // is TLB / page-table effects, which add only a few microseconds.
    latency_result bench_memmsg_roundtrip(int duration_ms)
    {
        std::string fwd = unique_name("lat_fwd");
        std::string rev = unique_name("lat_rev");

        mmb::memmsg ping_tx, ping_rx, pong_tx, pong_rx;
        if (!ping_tx.open(fwd, 4096, true,  true)  ||
            !ping_rx.open(fwd, 4096, false, false)  ||
            !pong_tx.open(rev, 4096, true,  true)   ||
            !pong_rx.open(rev, 4096, false, false))
            throw std::runtime_error("memmsg round-trip: open failed");

        std::atomic<bool> done{false};
        const std::string payload(64, 'p');

        std::thread echo([&] {
            while (!done.load(std::memory_order_acquire))
            {
                std::string msg = ping_rx.read(5);
                if (!msg.empty())
                    pong_tx.write(msg);
            }
        });

        // Warm up
        for (int i = 0; i < 20; i++)
        {
            ping_tx.write(payload);
            pong_rx.read(50);
        }

        latency_result r;
        r.name    = "memmsg round-trip latency";
        r.group   = "memmsg";
        r.payload = "64 B";

        auto end = clock_type::now() + std::chrono::milliseconds(duration_ms);

        while (clock_type::now() < end)
        {
            int64_t t0 = now_ns();
            ping_tx.write(payload);
            std::string resp = pong_rx.read(100);
            if (!resp.empty())
            {
                double us = (double)(now_ns() - t0) / 1000.0;
                if (us > 0.0 && us < 1000000.0)
                    r.samples_us.push_back(us);
            }
        }

        done.store(true, std::memory_order_release);
        // Unblock the echo thread if it is sleeping in read()
        ping_tx.write("stop");
        echo.join();

        std::sort(r.samples_us.begin(), r.samples_us.end());
        return r;
    }

    // ── JSON output ──────────────────────────────────────────────────────────

    void write_json(const std::string &path, const options &opt,
                    const std::vector<result> &results,
                    const std::vector<latency_result> &latency)
    {
        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("failed to open json output");

        auto now  = std::chrono::system_clock::now();
        auto t    = std::chrono::system_clock::to_time_t(now);

        out << "{\n";
        out << "  \"metadata\": {\n";
        out << "    \"duration_ms\": " << opt.duration_ms << ",\n";
        out << "    \"system\": \"" << json_escape(system_info()) << "\",\n";
        out << "    \"timestamp_unix\": " << (long long)t << "\n";
        out << "  },\n";

        // Throughput results (unchanged format)
        out << "  \"results\": [\n";
        for (size_t i = 0; i < results.size(); i++)
        {
            const auto &r = results[i];
            out << "    {";
            out << "\"name\":\"" << json_escape(r.name) << "\",";
            out << "\"group\":\"" << json_escape(r.group) << "\",";
            out << "\"payload\":\"" << json_escape(r.payload) << "\",";
            out << "\"operations\":" << r.operations << ",";
            out << "\"bytes\":" << r.bytes << ",";
            out << "\"seconds\":" << r.seconds << ",";
            out << "\"ops_per_sec\":" << r.ops_per_sec() << ",";
            out << "\"mb_per_sec\":" << r.mb_per_sec() << ",";
            out << "\"ns_per_op\":" << r.ns_per_op();
            out << "}";
            if (i + 1 != results.size()) out << ",";
            out << "\n";
        }
        out << "  ],\n";

        // Latency results
        out << "  \"latency_results\": [\n";
        for (size_t i = 0; i < latency.size(); i++)
        {
            const auto &r = latency[i];
            out << "    {";
            out << "\"name\":\"" << json_escape(r.name) << "\",";
            out << "\"group\":\"" << json_escape(r.group) << "\",";
            out << "\"payload\":\"" << json_escape(r.payload) << "\",";
            out << "\"samples\":" << r.count() << ",";
            out << "\"p50_us\":" << r.p50() << ",";
            out << "\"p95_us\":" << r.p95() << ",";
            out << "\"p99_us\":" << r.p99() << ",";
            out << "\"min_us\":" << r.min_us() << ",";
            out << "\"max_us\":" << r.max_us();
            out << "}";
            if (i + 1 != latency.size()) out << ",";
            out << "\n";
        }
        out << "  ]\n";
        out << "}\n";
    }

    // ── CLI ──────────────────────────────────────────────────────────────────

    options parse_options(int argc, char **argv)
    {
        options opt;
        for (int i = 1; i < argc; i++)
        {
            std::string arg = argv[i];
            if (arg == "--duration-ms" && i + 1 < argc)
                opt.duration_ms = std::max(100, std::atoi(argv[++i]));
            else if (arg == "--json" && i + 1 < argc)
                opt.json_path = argv[++i];
            else if (arg == "--help")
            {
                std::cout << "usage: bench-libmembus [--duration-ms N] [--json path]\n";
                std::exit(0);
            }
        }
        return opt;
    }
}

int main(int argc, char **argv)
{
    try
    {
        options opt = parse_options(argc, argv);
        std::vector<result> results;
        std::vector<latency_result> latency;

        // Throughput benchmarks
        results.push_back(bench_memmap(opt.duration_ms));
        results.push_back(bench_memmsg(opt.duration_ms));
        results.push_back(bench_memcmd(opt.duration_ms));
        results.push_back(bench_memkv_set(opt.duration_ms));
        results.push_back(bench_memkv_get(opt.duration_ms));
        results.push_back(bench_memvid(opt.duration_ms, 640, 480));
        results.push_back(bench_memvid(opt.duration_ms, 1920, 1080));
        results.push_back(bench_memaud(opt.duration_ms));
        results.push_back(bench_mempkt_write(opt.duration_ms, 1024));    // RTP-ish packet
        results.push_back(bench_mempkt_write(opt.duration_ms, 32768));   // JPEG-ish frame
        results.push_back(bench_mempkt_read(opt.duration_ms, 32768));    // copy-out cost

        // Latency benchmarks
        latency.push_back(bench_memmsg_roundtrip(opt.duration_ms));
        latency.push_back(bench_select_wakeup(opt.duration_ms));

        write_json(opt.json_path, opt, results, latency);

        std::cout << "\n--- Throughput ---\n";
        for (const auto &r : results)
            std::cout << r.name << " (" << r.payload << "): "
                      << (uint64_t)r.ops_per_sec() << " ops/s, "
                      << (uint64_t)r.mb_per_sec() << " MiB/s\n";

        std::cout << "\n--- Latency ---\n";
        for (const auto &r : latency)
            std::cout << r.name << " (" << r.payload << ", n=" << r.count() << "): "
                      << "p50=" << (int)r.p50() << " µs  "
                      << "p95=" << (int)r.p95() << " µs  "
                      << "p99=" << (int)r.p99() << " µs\n";

        std::cout << "\nwrote " << opt.json_path << "\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "benchmark failed: " << e.what() << "\n";
        return 1;
    }
}
