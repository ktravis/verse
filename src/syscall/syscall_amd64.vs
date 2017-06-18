sys_read            := 0;
sys_write           := 1;
sys_open            := 2;
sys_close           := 3;
sys_mmap            := 9;
sys_munmap          := 11;
sys_nanosleep       := 35;
sys_socket          := 41;
sys_connect         := 42;
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
sys_unlinkat        := 263;

MAP_SHARED     := 0x01;
MAP_PRIVATE    := 0x02;
MAP_TYPE       := 0x0f;
MAP_FIXED      := 0x10;
MAP_ANON       := 0x20;
MAP_ANONYMOUS  := MAP_ANON;
MAP_NORESERVE  := 0x4000;
MAP_GROWSDOWN  := 0x0100;
MAP_DENYWRITE  := 0x0800;
MAP_EXECUTABLE := 0x1000;
MAP_LOCKED     := 0x2000;
MAP_POPULATE   := 0x8000;
MAP_NONBLOCK   := 0x10000;
MAP_STACK      := 0x20000;
MAP_HUGETLB    := 0x40000;
MAP_FILE       := 0;

PROT_NONE      := 0;
PROT_READ      := 1;
PROT_WRITE     := 2;
PROT_EXEC      := 4;
PROT_GROWSDOWN := 0x01000000;
PROT_GROWSUP   := 0x02000000;

extern fn syscall(#autocast ptr) -> ptr;
extern fn syscall1(#autocast ptr, #autocast ptr) -> ptr;
extern fn syscall2(#autocast ptr, #autocast ptr, #autocast ptr) -> ptr;
extern fn syscall3(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;
extern fn syscall4(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;
extern fn syscall5(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;
extern fn syscall6(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;

fn exit(code:int) {
    syscall1(sys_exit, code);
}

fn exit_group(code:int) {
    syscall1(sys_exit_group, code);
}

fn kill(pid:int, sig:int) {
    syscall2(sys_kill, pid, sig);
}

fn read(fd: int, data: string, n:int) -> int {
    buf: ptr;
    return syscall3(sys_read, fd, buf, n) as int;
}

fn write(fd: int, data: string, n: int) -> int {
    return syscall3(sys_write, fd, data.bytes, n) as int;
}

// TODO: should be uint?
fn mmap(addr:int, len:int, prot:int, flags:int, fd:int, off:int) -> ptr {
    return syscall6(sys_mmap, addr, len, prot, flags, fd, off);
}

// TODO: should be uint?
fn munmap(addr:int, len:int) -> ptr {
    return syscall2(sys_munmap, addr, len);
}
