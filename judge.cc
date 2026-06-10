#include "judge.h"
#include "config.h"
#include "log.h"
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <seccomp.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <fstream>
#include <thread>
#include <ftw.h>

static std::string trim_trailing(const std::string& s) {
    size_t end = s.size();
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
                       s[end - 1] == '\n' || s[end - 1] == '\r'))
        end--;
    return s.substr(0, end);
}

static int rm_cb(const char* path, const struct stat*, int, struct FTW*) {
    remove(path);
    return 0;
}

static void rm_temp_dir(const std::string& dir) {
    if (!dir.empty() && (dir.rfind("/tmp/vibe-oj/", 0) == 0 || dir.rfind("./.judge-tmp/", 0) == 0))
        nftw(dir.c_str(), rm_cb, 64, FTW_DEPTH | FTW_PHYS);
}

static std::string run_cmd(const char* cmd, int timeout_sec) {
    int pipefd[2];
    if (pipe(pipefd) == -1) return "pipe failed";
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        execl("/bin/sh", "sh", "-c", cmd, nullptr);
        _exit(127);
    }
    close(pipefd[1]);
    std::string output;
    char buf[4096];
    auto start = std::chrono::steady_clock::now();
    while (true) {
        int status;
        pid_t r = waitpid(pid, &status, WNOHANG);
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            output += buf;
        }
        if (r != 0) {
            while ((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
                buf[n] = '\0';
                output += buf;
            }
            break;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_sec * 1000) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            output += "\n[compile timeout]";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    close(pipefd[0]);
    return output;
}

static int do_compile(const std::string& src, const std::string& bin, std::string& err) {
    std::string cmd = "g++ -std=c++17 -O2 -static -o " + bin + " " + src + " 2>&1";
    std::string out = run_cmd(cmd.c_str(), JUDGE_COMPILE_TIMEOUT_SEC);
    struct stat st;
    if (stat(bin.c_str(), &st) != 0 || st.st_size == 0) {
        err = out;
        return -1;
    }
    return 0;
}

static bool setup_seccomp() {
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (!ctx) return false;

    int allow_list[] = {
        SCMP_SYS(read), SCMP_SYS(write), SCMP_SYS(close),
        SCMP_SYS(fstat), SCMP_SYS(lseek), SCMP_SYS(mmap), SCMP_SYS(munmap),
        SCMP_SYS(mprotect), SCMP_SYS(brk), SCMP_SYS(mremap),
        SCMP_SYS(exit), SCMP_SYS(exit_group),
        SCMP_SYS(rt_sigaction), SCMP_SYS(rt_sigprocmask), SCMP_SYS(rt_sigreturn),
        SCMP_SYS(rt_sigsuspend),
        SCMP_SYS(arch_prctl), SCMP_SYS(set_tid_address), SCMP_SYS(set_robust_list),
        SCMP_SYS(futex), SCMP_SYS(madvise), SCMP_SYS(nanosleep),
        SCMP_SYS(readv), SCMP_SYS(writev), SCMP_SYS(clock_gettime),
        SCMP_SYS(getpid), SCMP_SYS(gettid), SCMP_SYS(getuid), SCMP_SYS(geteuid),
        SCMP_SYS(getgid), SCMP_SYS(getegid), SCMP_SYS(getrandom),
        SCMP_SYS(getpgrp), SCMP_SYS(getppid),
        SCMP_SYS(open), SCMP_SYS(openat), SCMP_SYS(access),
        SCMP_SYS(faccessat), SCMP_SYS(newfstatat), SCMP_SYS(readlink),
        SCMP_SYS(readlinkat),
        SCMP_SYS(stat), SCMP_SYS(lstat), SCMP_SYS(getcwd), SCMP_SYS(getdents64),
        SCMP_SYS(pread64), SCMP_SYS(sigaltstack), SCMP_SYS(sched_yield),
        SCMP_SYS(prlimit64), SCMP_SYS(uname), SCMP_SYS(sysinfo),
        SCMP_SYS(rseq), SCMP_SYS(ioctl), SCMP_SYS(fcntl),
        SCMP_SYS(clone), SCMP_SYS(wait4),
        SCMP_SYS(timer_create), SCMP_SYS(timer_settime),
        SCMP_SYS(dup2), SCMP_SYS(dup3), SCMP_SYS(setpgid), SCMP_SYS(pipe2),
        SCMP_SYS(execve),
        SCMP_SYS(shmctl), SCMP_SYS(shmget), SCMP_SYS(shmat), SCMP_SYS(shmdt),
        SCMP_SYS(chdir), SCMP_SYS(chroot), SCMP_SYS(tgkill),
    };

    for (int sys : allow_list) {
        if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, sys, 0) < 0) {
            seccomp_release(ctx);
            return false;
        }
    }

    if (seccomp_load(ctx) < 0) {
        seccomp_release(ctx);
        return false;
    }
    seccomp_release(ctx);
    return true;
}

static bool setup_rlimit() {
    struct rlimit rl;

    rl.rlim_cur = JUDGE_CPU_TIMEOUT_SEC;
    rl.rlim_max = JUDGE_CPU_TIMEOUT_SEC + 1;
    if (setrlimit(RLIMIT_CPU, &rl) != 0) return false;

    rlim_t mem_bytes = static_cast<rlim_t>(JUDGE_MEMORY_LIMIT_MB) * 1024 * 1024;
    rl.rlim_cur = mem_bytes;
    rl.rlim_max = mem_bytes;
    if (setrlimit(RLIMIT_AS, &rl) != 0) return false;

    rl.rlim_cur = 1;
    rl.rlim_max = 1;
    if (setrlimit(RLIMIT_NPROC, &rl) != 0) return false;

    rlim_t fsize = static_cast<rlim_t>(JUDGE_OUTPUT_LIMIT_MB) * 1024 * 1024;
    rl.rlim_cur = fsize;
    rl.rlim_max = fsize;
    setrlimit(RLIMIT_FSIZE, &rl);

    return true;
}

static void judge_in_child(const std::string&, const std::string& temp_dir,
                           int stdin_fd, int stdout_fd) {
    setup_rlimit();
    int rc = chdir(temp_dir.c_str());
    if (rc == 0) { int cr = chroot(temp_dir.c_str()); (void)cr; }
    setup_seccomp();

    dup2(stdin_fd, STDIN_FILENO);
    dup2(stdout_fd, STDOUT_FILENO);
    close(stdin_fd);
    close(stdout_fd);

    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }

    const char* args[] = {"./prog", nullptr};
    execve("./prog", const_cast<char* const*>(args), nullptr);
    _exit(1);
}

static std::string judge_single(const std::string& bin_path, const std::string& temp_dir,
                                const JudgeCase& tc, std::string& actual_out, int& time_ms) {
    int stdin_pipe[2], stdout_pipe[2];
    if (pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1)
        return "RE";

    auto t1 = std::chrono::steady_clock::now();
    pid_t pid = fork();

    if (pid == 0) {
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        judge_in_child(bin_path, temp_dir, stdin_pipe[0], stdout_pipe[1]);
    }

    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    ssize_t n = write(stdin_pipe[1], tc.input.data(), tc.input.size());
    (void)n;
    close(stdin_pipe[1]);

    auto start = std::chrono::steady_clock::now();
    int status;
    pid_t r;
    bool manual_kill = false;
    while (true) {
        r = waitpid(pid, &status, WNOHANG);
        if (r != 0) break;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > JUDGE_REAL_TIMEOUT_MS) {
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            manual_kill = true;
            r = pid;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    auto t2 = std::chrono::steady_clock::now();
    time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    char buf[4096];
    actual_out.clear();
    while (true) {
        ssize_t n = read(stdout_pipe[0], buf, sizeof(buf) - 1);
        if (n <= 0) break;
        buf[n] = '\0';
        actual_out += buf;
    }
    close(stdout_pipe[0]);

    if (manual_kill || (r != pid)) {
        return "TLE";
    }

    if (manual_kill) {
        return "TLE";
    }

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGXCPU) return "TLE";
        if (sig == SIGKILL) return "MLE";
        if (sig == SIGSEGV || sig == SIGABRT || sig == SIGFPE || sig == SIGBUS)
            return "RE";
        return "RE";
    }

    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) return "RE";

    std::string trimmed_actual = trim_trailing(actual_out);
    std::string trimmed_expected = trim_trailing(tc.expected);

    if (trimmed_actual == trimmed_expected)
        return "AC";

    actual_out = trimmed_actual;
    return "WA";
}

JudgeResult compile_and_judge(const std::string& code, const std::vector<JudgeCase>& cases) {
    JudgeResult result;
    result.status = "CE";

    if (cases.empty()) {
        result.compile_error = "No test cases provided";
        return result;
    }

    std::string temp_dir = std::string(JUDGE_TEMP_DIR) + "judge-XXXXXX";
    mkdir(JUDGE_TEMP_DIR, 0777);
    if (!mkdtemp(&temp_dir[0])) {
        result.compile_error = "Failed to create temp directory";
        return result;
    }

    std::string src_path = temp_dir + "/code.cpp";
    std::string bin_path = temp_dir + "/prog";

    {
        std::ofstream src(src_path);
        if (!src) {
            result.compile_error = "Failed to write source file";
            rm_temp_dir(temp_dir);
            return result;
        }
        src << code;
    }

    std::string compile_err;
    if (do_compile(src_path, bin_path, compile_err) != 0) {
        result.compile_error = compile_err.empty() ? "Compilation failed" : compile_err;
        rm_temp_dir(temp_dir);
        return result;
    }

    result.status = "AC";

    for (size_t i = 0; i < cases.size(); i++) {
        std::string actual;
        int t_ms = 0;
        std::string s = judge_single(bin_path, temp_dir, cases[i], actual, t_ms);
        result.time_ms += t_ms;

        if (s == "AC") continue;

        result.status = s;
        result.failed_case = static_cast<int>(i + 1);
        if (s == "WA") {
            result.expected_output = trim_trailing(cases[i].expected);
            result.actual_output = actual;
        }
        break;
    }

    rm_temp_dir(temp_dir);
    return result;
}
