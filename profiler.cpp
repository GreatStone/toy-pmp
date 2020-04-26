#include <gflags/gflags.h>
#include <libunwind.h>
#include <libunwind-ptrace.h>

#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#include <vector>
#include <list>
#include <stdio.h>
#include <dirent.h>
#include <endian.h>
#include <stdint.h>
#include <unordered_map>

typedef std::unordered_map<int32_t, void*> UptInfoMap;

DEFINE_int32(pid, 0, "The pid of target process.");
DEFINE_int32(attach, 0, "The method of attach child process.\n \
  0: no attach to child processes\n  1: attach all child processes as sync.\n \
  2: attach all child processes as async.\n");
DEFINE_int32(sample_times, 100, "The number of cycle that get stack of each process.");
DEFINE_int32(interval, 20, "The duration between samples in milliseconds.");

#define CHECK_NVAL_RET(val, exp, ret)                    \
    if ((val) != (exp)) {                                 \
        printf("Operation failed %s:%d "#val" != "#exp" errno=%d\n", __FILE__, __LINE__, errno); \
        return (ret);                                     \
    }


bool init_unwind(unw_accessors_t* accessor, unw_addr_space_t* addr_space) {
    *accessor = _UPT_accessors;
    *addr_space = unw_create_addr_space(accessor, __LITTLE_ENDIAN);
    if (*addr_space == nullptr) {
        return false;
    }
    return true;
}

bool attach_task(int32_t pid, bool* exited) {
    *exited = false;
    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) != 0) {
        if (errno == ESRCH) {
            *exited = true;
            return true;
        }
        return false;
    }
    return true;
}

bool detach_task(int32_t pid) {
    CHECK_NVAL_RET(ptrace(PTRACE_DETACH, pid, nullptr, nullptr), 0, false);
    return true;
}

bool list_child_task(int32_t pid, std::vector<int32_t>* child_list) {
    char task_path[50];
    if (snprintf(task_path, sizeof(task_path), "/proc/%d/task", pid) < 1) {
        return false;
    }
    DIR* dir = opendir(task_path);
    if (!dir) {
        return false;
    }
    struct dirent* entry = nullptr;
    while ((entry = readdir(dir)) != nullptr) {
        int32_t task_id = atoi(entry->d_name);
        if (task_id > 0) {
            child_list->push_back(task_id);
        }
    }
    closedir(dir);
    return true;
}

inline int64_t us_now() {
    struct timeval now;
    if (gettimeofday(&now, nullptr) < 0) {
        return -1;
    }

    return (int64_t)now.tv_sec * 1000000LL + (int64_t)now.tv_usec;
}

bool refresh_uptinfos(const std::vector<int32_t>& task_list, UptInfoMap* upt_infos) {
    for (const auto& pid : task_list) {
        auto iter = upt_infos->find(pid);
        if (iter == upt_infos->end()) {
            void* upt_info = _UPT_create(pid);
            if (upt_info == nullptr) {
                printf("Fail to create UPT tracer for %d\n", pid);
                return false;
            }
            upt_infos->insert(std::make_pair(pid, upt_info));
        }
    }
    return true;
}

bool sampling_stack(unw_addr_space_t* addr_space, void* upt_info, int32_t pid,
                    std::vector<unw_word_t>* stack) {
    unw_cursor_t cursor;

    CHECK_NVAL_RET(unw_init_remote(&cursor, *addr_space, upt_info), 0, false);
    stack->reserve(16);
    stack->push_back(pid);
    do {
        unw_word_t cur_ip = 0;
        unw_get_reg(&cursor, UNW_REG_IP, &cur_ip);
        stack->push_back(cur_ip);
    } while (unw_step(&cursor) > 0);
    return true;
}

bool summary_stacks(unw_addr_space_t* addr_space,
                    const std::list<std::vector<unw_word_t>>& sample_stack,
                    const UptInfoMap& upt_infos) {
    char buf[1024];
    std::unordered_map<std::string, int> stack_counter;
    for (const auto& stack : sample_stack) {
        int32_t pid = stack[0];
        std::string cur_record;
        for (int i = stack.size() - 1; i > 0; --i) {
            CHECK_NVAL_RET(_UPT_get_proc_name(*addr_space, stack[i], buf, sizeof(buf), nullptr, upt_infos.at(pid)), 0, false);
            cur_record.append(buf);
            cur_record.append(";");
        }
        auto iter = stack_counter.find(cur_record);
        if (iter == stack_counter.end()) {
            stack_counter[cur_record] = 1;
        } else {
            iter->second += 1;
        }
    }
    for (const auto& record : stack_counter) {
        printf("%s %d\n", record.first.data(), record.second);
    }
    return true;
}

int main(int argc, char* argv[]) {
    google::ParseCommandLineFlags(&argc, &argv, true);
    unw_accessors_t accessor;
    unw_addr_space_t addr_space;
    if (!init_unwind(&accessor, &addr_space)) {
        printf("init libunwind failed\n");
        return -1;
    }
    UptInfoMap upt_infos;
    std::list<std::vector<unw_word_t>> sample_stack;
    for (int i = 0; i < FLAGS_sample_times; ++i) {
        std::vector<int32_t> task_list;
        if (FLAGS_attach == 0) {
            task_list.push_back(FLAGS_pid);
        } else {
            if (!list_child_task(FLAGS_pid, &task_list)) {
                printf("Fail to fetch all child task of %d\n", FLAGS_pid);
                return -1;
            }
        }
        if (!refresh_uptinfos(task_list, &upt_infos)) {
            return -1;
        }

        int64_t now = us_now();
        if (FLAGS_attach == 1) { // attach all childs at once.
            bool exited = false;
            std::vector<int32_t> traceable_task;
            traceable_task.reserve(task_list.size());
            for (const auto& pid : task_list) {
                CHECK_NVAL_RET(attach_task(pid, &exited), true, -1);
                if (!exited) {
                    traceable_task.push_back(pid);
                }
            }
            for (const auto& pid : traceable_task) {
                CHECK_NVAL_RET(waitpid(pid, nullptr, __WALL), pid, -1);
            }
            for (const auto& pid : traceable_task) {
                std::vector<unw_word_t> frames;
                CHECK_NVAL_RET(sampling_stack(&addr_space, upt_infos[pid], pid, &frames), true, -1);
                sample_stack.push_back(std::move(frames));
                if (!detach_task(pid)) {
                    printf("detach %d failed\n", pid);
                }
            }
        } else { // attach and trace childs by turn.
            for (const auto& pid : task_list) {
                bool exited = false;
                CHECK_NVAL_RET(attach_task(pid, &exited), true, -1);
                if (exited) {
                    continue;
                }
                CHECK_NVAL_RET(waitpid(pid, nullptr, __WALL), pid, -1);
                std::vector<unw_word_t> frames;
                CHECK_NVAL_RET(sampling_stack(&addr_space, upt_infos[pid], pid, &frames), true, -1);
                sample_stack.push_back(std::move(frames));
                if (!detach_task(pid)) {
                    printf("detach %d failed\n", pid);
                }
            }
        }
        usleep(FLAGS_interval * 1000ul);
    }

    summary_stacks(&addr_space, sample_stack, upt_infos);
    return 0;
}
