#import "atomic"

mutexLocked := 1;

type Mutex: struct {
    state: i32;
}

fn lock(m:&Mutex) {
    while (atomic.SwapAndCompareInt32(m.state, 0, mutexLocked)) {
        futex_wait();
    }
}

fn unlock(m:&Mutex) {
}
