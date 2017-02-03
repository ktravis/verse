#import "syscall"

enum ClockTypes {
    CLOCK_REALTIME,
    CLOCK_MONOTONIC,
    CLOCK_PROCESS_CPUTIME_ID,
    CLOCK_THREAD_CPUTIME_ID,
    CLOCK_MONOTONIC_RAW,
    CLOCK_REALTIME_COARSE,
    CLOCK_MONOTONIC_COARSE,
    CLOCK_BOOTTIME,
    CLOCK_REALTIME_ALARM,
    CLOCK_BOOTTIME_ALARM,
    CLOCK_SGI_CYCLE,
    CLOCK_TAI,
};


type timer_t: ptr; // void *
type time_t: s32; // long
type suseconds_t: s32; // long

type timeval: struct {
    tv_sec: time_t;
    tv_usec: suseconds_t;
};

type timespec: struct {
    tv_sec: s64;
    tv_nsec: s64;
};

type tm: struct {
    tm_sec: int;
    tm_min: int;
    tm_hour: int;
    tm_mday: int;
    tm_mon: int;
    tm_year: int;
    tm_wday: int;
    tm_yday: int;
    tm_isdst: int;
    __tm_gmtoff: s64; // TODO: long is 64-bit?
    //__tm_zone: ptr;  // const char *
    __tm_zone: &u8;  // const char *
};

type itimerspec: struct {
    it_interval: timespec;
    it_value: timespec;
};

fn timer_gettime(t:timer_t):itimerspec {
    ts:itimerspec;
    syscall.syscall2(syscall.SYS_timer_gettime as ptr, t as ptr, &ts as ptr);
    return ts;
}

fn clock_gettime(clk:ClockTypes):timespec {
    ts:timespec;
    syscall.syscall2(syscall.SYS_clock_gettime as ptr, clk as ptr, &ts as ptr);
    return ts;
}

fn time():timespec {
    use ClockTypes;
    return clock_gettime(CLOCK_REALTIME);
}

fn sleep(n:int) {
    req := timespec::{tv_sec = n as s64};
    rem:timespec;
    syscall.syscall2(syscall.SYS_nanosleep as ptr, &req as ptr, &rem as ptr);
}

fn usleep(n:s64) {
    req := timespec::{
        tv_sec  = n/1000000,
        tv_nsec = (n % 1000000) * 1000
    };
    rem:timespec;
    syscall.syscall2(syscall.SYS_nanosleep as ptr, &req as ptr, &rem as ptr);
}

//fn time():float64 {
    //ts := clock_gettime(CLOCK_REALTIME);
    //f:float64 = (ts.tv_sec as float64) + ((ts.tv_nsec as float64) / 1000000000);
    //return f;
//}
