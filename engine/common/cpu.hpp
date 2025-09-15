#pragma once
#include <pthread.h>
#include <sched.h>
#include <string.h>
#include <string>
#include <stdexcept>

namespace cpu {

// Pin current thread to a specific CPU core (Linux)
inline void pin_this_thread(int cpu_index) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_index, &set);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    if (rc != 0) throw std::runtime_error("pthread_setaffinity_np: " + std::to_string(rc));
}

// Optional: set FIFO scheduling (requires CAP_SYS_NICE or sudo)
inline void set_realtime_fifo(int priority = 1) {
    sched_param sch{};
    sch.sched_priority = priority; // 1..99
    if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch) != 0) {
        // Don't throw; allow running without RT privileges
    }
}

// Optional: name the thread (best-effort)
inline void set_name(const char* name) {
#if defined(__linux__)
    pthread_setname_np(pthread_self(), name);
#else
    (void)name;
#endif
}

} // namespace cpu