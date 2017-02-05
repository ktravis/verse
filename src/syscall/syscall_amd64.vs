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

extern fn syscall(#autocast ptr);
extern fn syscall1(#autocast ptr, #autocast ptr) -> ptr;
extern fn syscall2(#autocast ptr, #autocast ptr, #autocast ptr) -> ptr;
extern fn syscall3(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;
extern fn syscall4(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;
extern fn syscall5(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;
extern fn __syscall6(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;

fn exit(code:int) {
    syscall1(sys_exit, code as ptr);
}

fn exit_group(code:int) {
    syscall1(sys_exit_group, code);
}

fn kill(pid:int, sig:int) {
    syscall2(sys_kill, pid, sig);
}

fn write(fd:int, data:string, n:int) {
    syscall3(sys_write, fd, data.bytes, n);
}

//fn mmap(addr:uint, len:uint, prot:uint, flags:uint, fd:uint, off:uint):ptr {
fn mmap(addr:int, len:int, prot:int, flags:int, fd:int, off:int):ptr {
    return __syscall6(sys_mmap, addr, len, prot, flags, fd, off);
}

//fn munmap(addr:uint, len:uint):ptr {
fn munmap(addr:int, len:int):ptr {
    return syscall2(sys_munmap, addr, len);
}
