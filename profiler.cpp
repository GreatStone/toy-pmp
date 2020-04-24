#include <gflags/gflags.h>
#include <libunwind.h>
#include <libunwind-ptrace.h>

#include <sys/ptrace.h>
#include <vector>
#include <stdio.h>
#include <endian.h>
#include <stdint.h>

DEFINE_int32(pid, 0, "The pid of target process.");

bool proc_unwind(int32_t pid) {
    unw_accessors_t accessor = _UPT_accessors;
    unw_addr_space_t addr_space = unw_create_addr_space(&accessor, __LITTLE_ENDIAN);
    if (addr_space == nullptr) {
        return false;
    }
    unw_cursor_t cursor;
    void* upt_info = _UPT_create(pid);
    ptrace(PTRACE_ATTACH, pid, nullptr, nullptr);
    int err = unw_init_remote(&cursor, addr_space, upt_info);
    if (err) {
        printf("init remote failed as errcode is %d\n", err);
    }
    std::vector<unw_word_t> backtrace;

    backtrace.clear();
    do {
        unw_word_t ip= 0;
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        backtrace.push_back(ip);
    } while (unw_step(&cursor) > 0);
    for (const auto& frame : backtrace) {
        char buf[1024];
        unw_word_t offp;
        _UPT_get_proc_name(addr_space, frame, buf, sizeof(buf), &offp, upt_info);
        printf("%s;", buf);
    }
    printf("\n");
    ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
    return true;
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    init_unwind(FLAGS_pid);
    return 0;
}
