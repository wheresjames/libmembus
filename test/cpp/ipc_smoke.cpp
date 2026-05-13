#include "libmembus.h"

#include <chrono>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    using namespace std::chrono_literals;

    std::string uniqueName(const char *suffix)
    {
        return std::string("/libmembus_ipc_") + std::to_string(getpid()) + "_" + suffix;
    }

    int childMemMap(const std::string &name, const std::string &expected)
    {
        mmb::memmap map;
        if (!map.open(name, 0, false, false))
        {
            std::cerr << "child failed to open memmap\n";
            return 10;
        }

        if (map.read(expected.size()) != expected)
        {
            std::cerr << "child read wrong memmap payload\n";
            return 11;
        }

        return 0;
    }

    int childMemMsg(const std::string &name, const std::string &expected)
    {
        mmb::memmsg rx;
        if (!rx.open(name, 1024, false, false))
        {
            std::cerr << "child failed to open memmsg\n";
            return 20;
        }

        std::string got = rx.read(3000);
        if (got != expected)
        {
            std::cerr << "child read wrong memmsg payload: " << got << "\n";
            return 21;
        }

        return 0;
    }

    int childMemKv(const std::string &name, const std::string &expected)
    {
        mmb::memkv kv;
        if (!kv.open(name))
        {
            std::cerr << "child failed to open memkv\n";
            return 30;
        }

        int64_t epoch = kv.getEpoch();
        auto changed = kv.getChanged(3000, epoch);
        if (changed["state"] != expected)
        {
            std::cerr << "child did not observe memkv change\n";
            return 31;
        }

        return 0;
    }

    int childMemCmd(const std::string &name, const std::string &expected)
    {
        mmb::memcmd cmd;
        if (!cmd.open(name, 1024, true, false))
        {
            std::cerr << "child failed to open memcmd\n";
            return 40;
        }

        std::string got = cmd.read(3000);
        if (got != expected)
        {
            std::cerr << "child read wrong memcmd payload: " << got << "\n";
            return 41;
        }

        return 0;
    }

    int waitForChild(pid_t pid)
    {
        for (int i = 0; i < 50; i++)
        {
            int status = 0;
            pid_t done = waitpid(pid, &status, WNOHANG);
            if (done == pid)
            {
                if (WIFEXITED(status))
                    return WEXITSTATUS(status);
                if (WIFSIGNALED(status))
                    return 128 + WTERMSIG(status);
                return 1;
            }

            if (done < 0)
                return 1;

            std::this_thread::sleep_for(100ms);
        }

        kill(pid, SIGKILL);
        waitpid(pid, nullptr, 0);
        std::cerr << "child timed out\n";
        return 124;
    }

    int runChild(const char *self, const char *mode, const std::string &name, const std::string &payload)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            std::cerr << "fork failed: " << std::strerror(errno) << "\n";
            return 1;
        }

        if (pid == 0)
        {
            execl(self, self, mode, name.c_str(), payload.c_str(), static_cast<char *>(nullptr));
            std::cerr << "exec failed: " << std::strerror(errno) << "\n";
            _exit(127);
        }

        return waitForChild(pid);
    }

    int runParent(const char *self)
    {
        {
            std::string name = uniqueName("memmap");
            std::string payload = "shared memory across exec";
            mmb::memmap map;
            if (!map.open(name, 256, true, true) || map.write(payload) != (int64_t)payload.size())
            {
                std::cerr << "parent failed to prepare memmap\n";
                return 2;
            }

            int rc = runChild(self, "--child-memmap", name, payload);
            if (rc != 0)
                return rc;
        }

        {
            std::string name = uniqueName("memmsg");
            std::string payload = "message across process boundary";
            mmb::memmsg tx;
            if (!tx.open(name, 1024, true, true))
            {
                std::cerr << "parent failed to create memmsg\n";
                return 3;
            }

            pid_t pid = fork();
            if (pid < 0)
            {
                std::cerr << "fork failed: " << std::strerror(errno) << "\n";
                return 1;
            }
            if (pid == 0)
            {
                execl(self, self, "--child-memmsg", name.c_str(), payload.c_str(), static_cast<char *>(nullptr));
                std::cerr << "exec failed: " << std::strerror(errno) << "\n";
                _exit(127);
            }

            std::this_thread::sleep_for(100ms);
            if (!tx.write(payload))
            {
                kill(pid, SIGKILL);
                waitpid(pid, nullptr, 0);
                std::cerr << "parent failed to write memmsg\n";
                return 4;
            }

            int rc = waitForChild(pid);
            if (rc != 0)
                return rc;
        }

        {
            std::string name = uniqueName("memkv");
            std::string payload = "running";
            mmb::memkv kv;
            if (!kv.create(name, 1, 16, 32, true) || !kv.setName(0, "state") || !kv.setValue("state", "idle"))
            {
                std::cerr << "parent failed to prepare memkv\n";
                return 5;
            }

            pid_t pid = fork();
            if (pid < 0)
            {
                std::cerr << "fork failed: " << std::strerror(errno) << "\n";
                return 1;
            }
            if (pid == 0)
            {
                execl(self, self, "--child-memkv", name.c_str(), payload.c_str(), static_cast<char *>(nullptr));
                std::cerr << "exec failed: " << std::strerror(errno) << "\n";
                _exit(127);
            }

            std::this_thread::sleep_for(100ms);
            if (!kv.setValue("state", payload))
            {
                kill(pid, SIGKILL);
                waitpid(pid, nullptr, 0);
                std::cerr << "parent failed to update memkv\n";
                return 6;
            }

            int rc = waitForChild(pid);
            if (rc != 0)
                return rc;
        }

        {
            std::string name = uniqueName("memcmd");
            std::string payload = "command across process boundary";
            mmb::memcmd cmd;
            if (!cmd.open(name, 1024, false, true))
            {
                std::cerr << "parent failed to create memcmd\n";
                return 7;
            }

            pid_t pid = fork();
            if (pid < 0)
            {
                std::cerr << "fork failed: " << std::strerror(errno) << "\n";
                return 1;
            }
            if (pid == 0)
            {
                execl(self, self, "--child-memcmd", name.c_str(), payload.c_str(), static_cast<char *>(nullptr));
                std::cerr << "exec failed: " << std::strerror(errno) << "\n";
                _exit(127);
            }

            for (int i = 0; i < 20 && cmd.readerCount() == 0; i++)
                std::this_thread::sleep_for(50ms);

            if (cmd.readerCount() != 1 || !cmd.write(payload))
            {
                kill(pid, SIGKILL);
                waitpid(pid, nullptr, 0);
                std::cerr << "parent failed to write memcmd\n";
                return 8;
            }

            int rc = waitForChild(pid);
            if (rc != 0)
                return rc;
            if (cmd.readerCount() != 0)
            {
                std::cerr << "memcmd reader count did not decrement\n";
                return 9;
            }
        }

        return 0;
    }
}

int main(int argc, char **argv)
{
    if (argc == 4 && std::string(argv[1]) == "--child-memmap")
        return childMemMap(argv[2], argv[3]);
    if (argc == 4 && std::string(argv[1]) == "--child-memmsg")
        return childMemMsg(argv[2], argv[3]);
    if (argc == 4 && std::string(argv[1]) == "--child-memkv")
        return childMemKv(argv[2], argv[3]);
    if (argc == 4 && std::string(argv[1]) == "--child-memcmd")
        return childMemCmd(argv[2], argv[3]);
    if (argc == 1)
        return runParent(argv[0]);

    std::cerr << "usage: " << argv[0] << "\n";
    return 64;
}
