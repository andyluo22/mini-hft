#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
#include <chrono>

#include "../engine/spsc/spsc_ring.hpp"
#include "../engine/common/timebase.hpp"
#include "../engine/common/cpu.hpp"

#if defined(__x86_64__) || defined(_M_X64)
  #include <immintrin.h>
  static inline void cpu_relax() { _mm_pause(); }
#else
  static inline void cpu_relax() {}
#endif  // <-- close the preprocessor block cleanly

// Define the Args struct (this is YOUR custom type, not built-in)
struct Args {
    int seconds = 3;
    std::size_t capacity = 1u << 20; // 1,048,576 slots
    int prod_cpu = -1;
    int cons_cpu = -1;
};

static Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--seconds") && i+1 < argc) a.seconds = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--cap") && i+1 < argc) a.capacity = std::stoull(argv[++i]);
        else if (!std::strcmp(argv[i], "--pin-prod") && i+1 < argc) a.prod_cpu = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--pin-cons") && i+1 < argc) a.cons_cpu = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--help")) {
            std::cout <<
              "Usage: spsc_bench [--seconds N] [--cap POW2] [--pin-prod CPU] [--pin-cons CPU]\n";
            std::exit(0);
        }
    }
    return a;
}

int main(int argc, char** argv) {
    auto args = parse_args(argc, argv);
    SpscRing<uint32_t> q(args.capacity);

    std::atomic<bool> start{false}; //in main thread (accessed by prod and cons thread)
    std::atomic<bool> stop{false}; // in main thread (accessed by prod and cons thread)

    uint64_t prod_cnt = 0, cons_cnt = 0;

    std::thread prod([&]{ // thread operates outside of main block
        if (args.prod_cpu >= 0) cpu::pin_this_thread(args.prod_cpu);
        cpu::set_name("producer");
        uint32_t x = 0; // monotonic counter {0,1,2,3,4,5...} for verification
        while (!start.load(std::memory_order_acquire)) cpu_relax(); //wait for green lit
        while (!stop.load(std::memory_order_relaxed)) { // stop when stop = True
            if (q.try_push(x)) {
                ++prod_cnt; ++x;
            } else {
                cpu_relax();
            }
        }
    });

    std::thread cons([&]{ // thread operates outside of main block
        if (args.cons_cpu >= 0) cpu::pin_this_thread(args.cons_cpu);
        cpu::set_name("consumer");
        uint32_t out;
        while (!start.load(std::memory_order_acquire)) cpu_relax(); //wait for green lit
        while (!stop.load(std::memory_order_relaxed)) {
            if (q.try_pop(out)) {
                ++cons_cnt;
            } else {
                cpu_relax();
            }
        }
    });

    // Warmup + timed run
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); // main/current thread
    start.store(true, std::memory_order_release); // signal GREEN for produ and cons to do work

    tb::Stopwatch sw;
    while (sw.elapsed_sec() < args.seconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    stop.store(true, std::memory_order_release); // signal to stop

    prod.join(); // wait for threads to finish 
    cons.join(); // wait for threads to finish
		
		// compute and print stats
    double secs = sw.elapsed_sec();
    double ops = double(cons_cnt); // count pops (completed msgs)
    double mops = ops / 1e6 / secs;
    std::cout << "SPSC: " << ops << " msgs in " << secs << " s → "
              << mops << " Mops/s\n"
              << "produced=" << prod_cnt << " consumed=" << cons_cnt
              << " backlog=" << (prod_cnt - cons_cnt) << "\n";
    return 0;
}
