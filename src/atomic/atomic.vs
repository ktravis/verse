// atomic

extern fn a_cas(&int, int, int) -> int;
extern fn a_swap(&int, int) -> s64;
extern fn a_spin();
extern fn a_incr(&int);
extern fn a_decr(&int);
extern fn a_store(&int, int);

// TODO: Is it 32-bit? should rewrite in C and make sure with stdint. should definitely be careful here, the system probably has an expectation of what int is and that provides a particular guarantee but only for amd64, so moving outside of this should probably not rely on the implicit system size, rather should call out numeric sizes everywhere. This will be key for future for adding additional platforms.
fn compareAndSwapInt32(p:&int, t:int, s:int) -> int {
    return a_cas(p, t, s);
}

fn swapInt64(p:&s64, v:s64) -> s64 {
    return a_swap(p as &int, v as int);
}

fn spin() {
    a_spin();
}

fn incr(p:&int) {
    a_incr(p);
}

fn decr(p:&int) {
    a_decr(p);
}

fn store(p:&int, x:int) {
    a_store(p, x);
}
