#pragma once
#include <string>
#include <stdexcept>

#if defined(__linux__)
  #include <pthread.h>
  #include <sched.h>
  #include <string.h> // strerror
#endif

namespace cpu {

// Pin current thread to a specific CPU core (Linux). Throws on failure.
inline void pin_this_thread(int cpu_index) {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu_index, &set);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(set), &set);
    if (rc != 0) {
      throw std::runtime_error(
        std::string("pthread_setaffinity_np failed (rc=") +
        std::to_string(rc) + "): " + strerror(rc));
    }
#else
    (void)cpu_index; // no-op on non-Linux
#endif
}

// Optional: set FIFO scheduling (requires CAP_SYS_NICE or sudo). Best-effort.
inline void set_realtime_fifo(int priority = 1) {
#if defined(__linux__)
    sched_param sch{};
    sch.sched_priority = priority; // 1..99
    (void)pthread_setschedparam(pthread_self(), SCHED_FIFO, &sch);
#else
    (void)priority;
#endif
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

// ---- Backwards-compat free helpers used by existing bench code ----
inline void pin_thread_to_cpu(int cpu_index) { cpu::pin_this_thread(cpu_index); }
inline void set_thread_realtime_fifo(int priority = 1) { cpu::set_realtime_fifo(priority); }
inline void set_thread_name(const char* name) { cpu::set_name(name); }

// ---- Tiny toolkit shim so bench code can call tb::pin_thread_to_cpu ----
namespace tb {
inline void pin_thread_to_cpu(int cpu_index) { cpu::pin_this_thread(cpu_index); }
inline void set_thread_realtime_fifo(int priority = 1) { cpu::set_realtime_fifo(priority); }
inline void set_thread_name(const char* name) { cpu::set_name(name); }
} // namespace tb
