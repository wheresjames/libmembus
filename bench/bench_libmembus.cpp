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

    struct result
    {
        std::string name;
        std::string group;
        std::string payload;
        uint64_t operations = 0;
        uint64_t bytes = 0;
        double seconds = 0.0;

        double ops_per_sec() const { return seconds > 0.0 ? (double)operations / seconds : 0.0; }
        double mb_per_sec() const { return seconds > 0.0 ? ((double)bytes / (1024.0 * 1024.0)) / seconds : 0.0; }
        double ns_per_op() const { return operations > 0 ? seconds * 1000000000.0 / (double)operations : 0.0; }
    };

    struct options
    {
        int duration_ms = 1000;
        std::string json_path = "bench/results/latest.json";
    };

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
            if (c == '\\' || c == '"')
            {
                out.push_back('\\');
                out.push_back(c);
            }
            else if (c == '\n')
                out += "\\n";
            else
                out.push_back(c);
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
        r.name = name;
        r.group = group;
        r.payload = payload;

        // Warm up briefly so one-time page faults and setup noise do not dominate.
        auto warm_end = clock_type::now() + std::chrono::milliseconds(std::max(50, duration_ms / 10));
        while (clock_type::now() < warm_end)
            fn();

        auto start = clock_type::now();
        auto end = start + std::chrono::milliseconds(duration_ms);
        while (clock_type::now() < end)
        {
            auto bytes = fn();
            r.operations++;
            r.bytes += bytes;
        }
        r.seconds = std::chrono::duration<double>(clock_type::now() - start).count();
        return r;
    }

    result bench_memmap(int duration_ms)
    {
        constexpr size_t payload_size = 64 * 1024;
        std::string payload(payload_size, 'm');
        mmb::memmap map;
        if (!map.open(unique_name("memmap"), payload_size, true, true))
            throw std::runtime_error("memmap open failed");

        return run_for("memmap read/write", "memmap", "64 KiB", duration_ms, [&]() -> uint64_t {
            map.write(payload);
            volatile char c = map.data()[0];
            (void)c;
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
        for (int i = 0; i < 8; i++)
        {
            kv.setName(i, "k" + std::to_string(i));
            kv.setValue(i, "value");
        }

        int v = 0;
        return run_for("memkv getValue", "memkv", "64 B value", duration_ms, [&]() -> uint64_t {
            volatile auto s = kv.getValue(v % 8);
            (void)s;
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

    void write_json(const std::string &path, const options &opt, const std::vector<result> &results)
    {
        std::ofstream out(path);
        if (!out)
            throw std::runtime_error("failed to open json output");

        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);

        out << "{\n";
        out << "  \"metadata\": {\n";
        out << "    \"duration_ms\": " << opt.duration_ms << ",\n";
        out << "    \"system\": \"" << json_escape(system_info()) << "\",\n";
        out << "    \"timestamp_unix\": " << (long long)t << "\n";
        out << "  },\n";
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
            if (i + 1 != results.size())
                out << ",";
            out << "\n";
        }
        out << "  ]\n";
        out << "}\n";
    }

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
        results.push_back(bench_memmap(opt.duration_ms));
        results.push_back(bench_memmsg(opt.duration_ms));
        results.push_back(bench_memcmd(opt.duration_ms));
        results.push_back(bench_memkv_set(opt.duration_ms));
        results.push_back(bench_memkv_get(opt.duration_ms));
        results.push_back(bench_memvid(opt.duration_ms, 640, 480));
        results.push_back(bench_memvid(opt.duration_ms, 1920, 1080));
        results.push_back(bench_memaud(opt.duration_ms));

        write_json(opt.json_path, opt, results);

        for (const auto &r : results)
            std::cout << r.name << " (" << r.payload << "): "
                      << (uint64_t)r.ops_per_sec() << " ops/s, "
                      << (uint64_t)r.mb_per_sec() << " MiB/s\n";
        std::cout << "wrote " << opt.json_path << "\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "benchmark failed: " << e.what() << "\n";
        return 1;
    }
}
