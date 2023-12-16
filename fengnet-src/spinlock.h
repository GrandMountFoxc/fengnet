// 用atomic_flag简单地实现一个自旋锁
// 可以用lock_guard和unique_lock自动加锁解锁

#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <atomic>
#include <thread>

#include "fengnet_malloc.h"

using namespace std;

class SpinLock{
public:
    SpinLock() = default;
    SpinLock(const SpinLock&) = delete;
    SpinLock& operator=(const SpinLock) = delete;

    // lock() unlock()应该是lock_guard加锁解锁时固定的API
    void lock(){
        while(flag.test_and_set(std::memory_order_relaxed)){
            std::this_thread::yield();
        }
    }

    int trylock() {
        return flag.test_and_set(std::memory_order_relaxed)==0;
        // return atomic_flag_test_and_set_(&lock->lock) == 0;
    }

    void unlock(){
        flag.clear(std::memory_order_relaxed);
    }
private:
    atomic_flag flag;
};

#endif