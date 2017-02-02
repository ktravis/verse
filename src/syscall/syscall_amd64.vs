SYS_write         := 1;
SYS_open          := 2;
SYS_close         := 3;
SYS_nanosleep         := 35;
SYS_exit          := 60;
SYS_kill          := 62;
SYS_fcntl         := 72;
SYS_gettimeofday  := 96;
SYS_timer_gettime := 224;
SYS_exit_group    := 231;
SYS_clock_gettime := 228;
SYS_clock_nanosleep    := 230;

// TODO: can't make declaration the same name as package gets error:
// Cannot use dot operator on non-struct type 'fn(ptr)'.
// extern fn syscall(ptr);
extern fn syscall1(ptr, ptr);
extern fn syscall2(ptr, ptr, ptr);
extern fn syscall3(ptr, ptr, ptr, ptr):ptr;

fn exit(code:int) {
    syscall1(SYS_exit as ptr, code as ptr);
}

fn exit_group(code:int) {
    syscall1(SYS_exit_group as ptr, code as ptr);
}

fn kill(pid:int, sig:int) {
    syscall2(SYS_kill as ptr, pid as ptr, sig as ptr);
}

fn write(fd:int, data:string, n:int) {
    syscall3(SYS_write as ptr, fd as ptr, data.bytes as ptr, n as ptr);
}
