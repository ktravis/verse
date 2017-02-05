SYS_write           := 1;
SYS_open            := 2;
SYS_close           := 3;
SYS_nanosleep       := 35;
SYS_exit            := 60;
SYS_kill            := 62;
SYS_fcntl           := 72;
SYS_gettimeofday    := 96;
SYS_timer_gettime   := 224;
SYS_exit_group      := 231;
SYS_clock_gettime   := 228;
SYS_clock_nanosleep := 230;

extern fn syscall(#autocast ptr);
extern fn syscall1(#autocast ptr, #autocast ptr);
extern fn syscall2(#autocast ptr, #autocast ptr, #autocast ptr);
extern fn syscall3(#autocast ptr, #autocast ptr, #autocast ptr, #autocast ptr) -> ptr;

fn exit(code:int) {
    syscall1(SYS_exit, code);
}

fn exit_group(code:int) {
    syscall1(SYS_exit_group, code);
}

fn kill(pid:int, sig:int) {
    syscall2(SYS_kill, pid, sig);
}

fn write(fd:int, data:string, n:int) {
    syscall3(SYS_write, fd, data.bytes, n);
}
