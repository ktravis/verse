#import "atomic"
#import "fmt"
#import "syscall"

mutexLocked := 1;

type Mutex: s64;

fn lock(m:&Mutex) {
    while (true) {
        if atomic.swapInt64(m as &s64, 1) != 1 {
            break;
        }
        n := *(m as &int) + 1;
        futex_wait(m as &s64, &n as &s64, 1, 1);
    }
}

fn unlock(m:&Mutex) {
    if *m != 0 {
        atomic.store(m as &int, 0);
        n := *(m as &int) + 1;
        if n != 0 {
            futex_wakeup(m as &s64, 1, 1);
        }

    }
}

FUTEX_PRIVATE := 128;
FUTEX_CLOCK_REALTIME := 256;

enum FutexState {
    FUTEX_WAIT,
    FUTEX_WAKE,
    FUTEX_FD,
    FUTEX_REQUEUE,
    FUTEX_CMP_REQUEUE,
    FUTEX_WAKE_OP,
    FUTEX_LOCK_PI,
    FUTEX_UNLOCK_PI,
    FUTEX_TRYLOCK_PI,
    FUTEX_WAIT_BITSET,
};

INT_MAX := 0x7fffffff;

fn futex_wait(addr:&s64, waiters:&s64, val:s64, priv:s64) {
    spins := 100;
    if priv != 0 {
        priv = FUTEX_PRIVATE as s64;
    }
    // TODO: Cannot perform logical negation on type '&s64' or 's64'
    //while (!waiters || !*waiters) {
    while (*waiters != 0) {
        spins -= 1;
        if spins == 0 {
            break;
        }
        if (*addr == val) {
            atomic.spin();
        } else {
            return;
        }
    }
    // TODO: validptr? can't just check if null?
    if (validptr(waiters as ptr)) {
        atomic.incr(waiters as &int);
    }
    while (*addr == val) {
        r := syscall.syscall4(syscall.sys_futex as ptr, addr as ptr, (FutexState.FUTEX_WAIT as int|priv) as ptr, val as ptr, 0 as ptr);
        if (r as int != -syscall.ENOSYS) {
            continue;
        }
        syscall.syscall4(syscall.sys_futex as ptr, addr as ptr, FutexState.FUTEX_WAIT as ptr, val as ptr, 0 as ptr);
    }
    if (validptr(waiters as ptr)) {
        atomic.decr(waiters as &int);
    }
}

fn futex_wakeup(addr:&s64, cnt:s64, priv:s64) {
    if priv != 0 {
        priv = FUTEX_PRIVATE as s64;
    }
    if cnt < 0 {
        cnt = INT_MAX as s64;
    }
    r := syscall.syscall3(syscall.sys_futex as ptr, addr as ptr, (FutexState.FUTEX_WAKE as int|priv) as ptr, cnt as ptr);
    if (r as int != -syscall.ENOSYS) {
        return;
    }
    syscall.syscall3(syscall.sys_futex as ptr, addr as ptr, FutexState.FUTEX_WAKE as ptr, cnt as ptr);
}
