// engine/spsc/spsc_ring.hpp
#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <new>
#include <utility>
#include <stdexcept>  // for std::invalid_argument

#ifndef CACHELINE_SIZE
#define CACHELINE_SIZE 64
#endif
struct alignas(CACHELINE_SIZE) CachelinePad { char pad[CACHELINE_SIZE]; };

constexpr bool is_pow2(std::size_t x) { return x && ((x & (x - 1)) == 0); }


template<typename T>
class SpscRing {
public:
    explicit SpscRing(std::size_t capacity_pow2)
      : cap_(capacity_pow2), mask_(capacity_pow2 - 1),
        buf_(static_cast<T*>(::operator new(sizeof(T) * capacity_pow2))) {
        if (!is_pow2(capacity_pow2)) {
            ::operator delete(buf_);
            throw std::invalid_argument("SpscRing capacity must be power-of-two");
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    ~SpscRing() {
        // Destroy remaining constructed elements (if any)
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        std::size_t head = head_.load(std::memory_order_relaxed);
        while (tail != head) {
            std::size_t idx = tail & mask_;
            buf_[idx].~T(); // call destructor for element T in that slot
            ++tail;
        }
        ::operator delete(buf_); // free space after we remove elements
    }

    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;

    // Non-blocking push. Returns false if ring is full.
    template<typename U>
    bool try_push(U&& v) {
        static_assert(std::is_move_constructible<T>::value || std::is_copy_constructible<T>::value,
                      "T must be move/copy constructible");

        std::size_t head = head_.load(std::memory_order_relaxed);
        std::size_t tail = tail_.load(std::memory_order_acquire); // pairs with consumer's release

        if (head - tail == cap_) [[unlikely]]
            return false; // full

        std::size_t idx = head & mask_;
        new (buf_ + idx) T(std::forward<U>(v));            // construct in-place
        head_.store(head + 1, std::memory_order_release);  // publish after construction
        return true;
    }

    // Non-blocking pop. Returns false if ring is empty.
    bool try_pop(T& out) {
        std::size_t tail = tail_.load(std::memory_order_relaxed);
        std::size_t head = head_.load(std::memory_order_acquire); // see producer's release

        if (head == tail) [[unlikely]]
            return false; // empty

        std::size_t idx = tail & mask_;
        T* ptr = buf_ + idx;
        out = std::move(*ptr);
        ptr->~T();                                           // destroy moved-from
        tail_.store(tail + 1, std::memory_order_release);    // publish
        return true;
    }

    // Optional: bulk helpers (best-effort)

    // Generates up to max_n items via producer_fn() and pushes them.
    template<typename F>
    std::size_t try_push_bulk(std::size_t max_n, F producer_fn) {
        std::size_t pushed = 0;
        for (; pushed < max_n; ++pushed) {
            using R = std::invoke_result_t<F>;
            static_assert(std::is_same_v<std::decay_t<R>, T>,
                          "producer_fn must return T");
            R val = producer_fn();
            if (!try_push(std::move(val)))
                break;
        }
        return pushed;
    }

    // Pops up to max_n items and passes each to consumer_fn(T&&).
    // This version avoids requiring T to be default-constructible.
    template<typename F>
    std::size_t try_pop_bulk(std::size_t max_n, F consumer_fn) {
        std::size_t popped = 0;
        for (; popped < max_n; ++popped) {
            std::size_t tail = tail_.load(std::memory_order_relaxed);
            std::size_t head = head_.load(std::memory_order_acquire);
            if (head == tail) break;

            std::size_t idx = tail & mask_;
            T* ptr = buf_ + idx;
            consumer_fn(std::move(*ptr));
            ptr->~T();
            tail_.store(tail + 1, std::memory_order_release);
        }
        return popped;
    }

    bool empty() const { return size() == 0; }
    bool full()  const { return size() == cap_; }

    std::size_t size() const {
        std::size_t head = head_.load(std::memory_order_acquire);
        std::size_t tail = tail_.load(std::memory_order_acquire);
        return head - tail;
    }

    std::size_t capacity() const { return cap_; }

private:
    CachelinePad _p0;
    alignas(CACHELINE_SIZE) std::atomic<std::size_t> head_{0};
    CachelinePad _p1; // separate head/tail to different cache lines
    alignas(CACHELINE_SIZE) std::atomic<std::size_t> tail_{0};
    CachelinePad _p2;

    const std::size_t cap_;
    const std::size_t mask_;
    T* const buf_;
};
