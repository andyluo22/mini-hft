// bench/spsc_backpressure_bench.cpp
#include <atomic>
#include <cstdint>
#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <chrono>

#include "../engine/spsc/spsc_channel.hpp"
#include "../engine/common/timebase.hpp"
#include "../engine/common/cpu.hpp"

struct Args {
  int seconds = 5;
  std::size_t capacity = 1u << 18; // 262,144
  std::size_t high_wm  = (1u << 18) * 3 / 4;
  std::size_t low_wm   = (1u << 18) / 2;
  int prod_cpu = -1, cons_cpu = -1;
  BpMode mode = BpMode::Drop;
  int consumer_slow_ns = 0; // simulate slowness per pop
};

static Args parse_args(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    if (!std::strcmp(argv[i], "--seconds") && i+1 < argc) a.seconds = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--cap") && i+1 < argc) a.capacity = std::stoull(argv[++i]);
    else if (!std::strcmp(argv[i], "--high") && i+1 < argc) a.high_wm = std::stoull(argv[++i]);
    else if (!std::strcmp(argv[i], "--low")  && i+1 < argc) a.low_wm  = std::stoull(argv[++i]);
    else if (!std::strcmp(argv[i], "--pin-prod") && i+1 < argc) a.prod_cpu = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--pin-cons") && i+1 < argc) a.cons_cpu = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--mode") && i+1 < argc) {
      std::string m = argv[++i];
      if (m == "drop") a.mode = BpMode::Drop;
      else if (m == "spin") a.mode = BpMode::Spin;
      else if (m == "sleep") a.mode = BpMode::Sleep;
    }
    else if (!std::strcmp(argv[i], "--cons-slow-ns") && i+1 < argc) a.consumer_slow_ns = std::atoi(argv[++i]);
    else if (!std::strcmp(argv[i], "--help")) {
      std::cout <<
        "Usage: spsc_backpressure_bench [--seconds N] [--cap POW2]\n"
        "       [--high N] [--low N] [--mode drop|spin|sleep]\n"
        "       [--cons-slow-ns N] [--pin-prod CPU] [--pin-cons CPU]\n";
      std::exit(0);
    }
  }
  return a;
}

int main(int argc, char** argv) {
  auto args = parse_args(argc, argv);
  SpscStats stats;
  BackpressureCfg cfg{args.high_wm, args.low_wm, args.mode, 5000};
  SpscChannel<uint32_t> ch(args.capacity, cfg, &stats);

  std::atomic<bool> start{false}, stop{false};
  uint64_t prod_cnt = 0, cons_cnt = 0;

  std::thread prod([&]{
    if (args.prod_cpu >= 0) cpu::pin_this_thread(args.prod_cpu);
    cpu::set_name("producer");
    uint32_t x = 0;
    while (!start.load(std::memory_order_acquire)) { cpu_relax(); }
    while (!stop.load(std::memory_order_relaxed)) {
      if (ch.push(x, &stop)) { ++prod_cnt; ++x; }
    }
  });

  std::thread cons([&]{
    if (args.cons_cpu >= 0) cpu::pin_this_thread(args.cons_cpu);
    cpu::set_name("consumer");
    uint32_t out;
    auto slow = std::chrono::nanoseconds(args.consumer_slow_ns);
    while (!start.load(std::memory_order_acquire)) {cpu_relax();}
    while (!stop.load(std::memory_order_relaxed)) {
      if (ch.pop(out)) {
        ++cons_cnt; // simulate consumer doing work (sleep for slow)
        if (args.consumer_slow_ns > 0) std::this_thread::sleep_for(slow);
      }
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  start.store(true, std::memory_order_release); // start start flag

  tb::Stopwatch sw;
  while (sw.elapsed_sec() < args.seconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  stop.store(true, std::memory_order_release); // start stop flag 
  prod.join(); cons.join();

  const double secs = sw.elapsed_sec();
  const double mops = double(cons_cnt) / 1e6 / secs;

  std::cout << "mode=" << (args.mode==BpMode::Drop?"drop":args.mode==BpMode::Spin?"spin":"sleep")
            << " cap=" << args.capacity
            << " high=" << args.high_wm << " low=" << args.low_wm
            << " cons_slow_ns=" << args.consumer_slow_ns << "\n";
  std::cout << "consumed=" << cons_cnt << " in " << secs << " s → " << mops << " Mops/s\n";
  std::cout << "produced=" << prod_cnt << " drops=" << stats.drops_total.load()
            << " max_depth=" << stats.max_depth.load()
            << " depth_now=" << stats.depth_gauge.load() << "\n";
  return 0;
}

