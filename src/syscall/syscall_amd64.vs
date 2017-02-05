sys_write           := 1;
sys_open            := 2;
sys_close           := 3;
sys_mmap            := 9;
sys_munmap          := 11;
sys_nanosleep       := 35;
sys_clone           := 56;
sys_exit            := 60;
sys_kill            := 62;
sys_fcntl           := 72;
sys_gettimeofday    := 96;
sys_futex           := 202;
sys_timer_gettime   := 224;
sys_exit_group      := 231;
sys_clock_gettime   := 228;
sys_clock_nanosleep := 230;

// TODO: can't make declaration the same name as package gets error:
// Cannot use dot operator on non-struct type 'fn(ptr)'.
// extern fn syscall(ptr);
extern fn syscall1(ptr, ptr):ptr;
extern fn syscall2(ptr, ptr, ptr):ptr;
extern fn syscall3(ptr, ptr, ptr, ptr):ptr;
extern fn syscall4(ptr, ptr, ptr, ptr, ptr):ptr;
extern fn syscall5(ptr, ptr, ptr, ptr, ptr, ptr):ptr;
extern fn __syscall6(ptr, ptr, ptr, ptr, ptr, ptr, ptr):ptr;

fn exit(code:int) {
    syscall1(sys_exit as ptr, code as ptr);
}

fn exit_group(code:int) {
    syscall1(sys_exit_group as ptr, code as ptr);
}

fn kill(pid:int, sig:int) {
    syscall2(sys_kill as ptr, pid as ptr, sig as ptr);
}

fn write(fd:int, data:string, n:int) {
    syscall3(sys_write as ptr, fd as ptr, data.bytes as ptr, n as ptr);
}

//fn mmap(addr:uint, len:uint, prot:uint, flags:uint, fd:uint, off:uint):ptr {
fn mmap(addr:int, len:int, prot:int, flags:int, fd:int, off:int):ptr {
    return __syscall6(sys_mmap as ptr, addr as ptr, len as ptr, prot as ptr, flags as ptr, fd as ptr, off as ptr);
}

//fn munmap(addr:uint, len:uint):ptr {
fn munmap(addr:int, len:int):ptr {
    return syscall2(sys_munmap as ptr, addr as ptr, len as ptr);
}
