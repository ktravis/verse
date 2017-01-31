extern fn syscall3(ptr, ptr, ptr, ptr);

// SYS_write
fn write(fd:int, data:string, n:int) {
    syscall3(1 as ptr, fd as ptr, data.bytes as ptr, n as ptr);
}
