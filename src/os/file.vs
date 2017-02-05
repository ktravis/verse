#import "syscall"
#import "os"

O_CREAT     :=     0o100;
O_EXCL      :=     0o200;
O_NOCTTY    :=     0o400;
O_TRUNC     :=    0o1000;
O_APPEND    :=    0o2000;
O_NONBLOCK  :=    0o4000;
O_DSYNC     :=   0o10000;
O_SYNC      := 0o4010000;
O_RSYNC     := 0o4010000;
O_DIRECTORY :=  0o200000;
O_NOFOLLOW  :=  0o400000;
O_CLOEXEC   := 0o2000000;

O_ASYNC     :=    0o20000;
O_DIRECT    :=    0o40000;
O_LARGEFILE :=   0o100000;
O_NOATIME   :=  0o1000000;
O_PATH      := 0o10000000;
O_TMPFILE   := 0o20200000;
O_NDELAY    := O_NONBLOCK;

O_RDONLY := 0o0;
O_WRONLY := 0o1;
O_RDWR   := 0o2;

FD_CLOEXEC := 1;

F_DUPFD := 0;
F_GETFD := 1;
F_SETFD := 2;
F_GETFL := 3;
F_SETFL := 4;

F_SETOWN := 8;
F_GETOWN := 9;
F_SETSIG := 10;
F_GETSIG := 11;

F_GETLK  := 12;
F_SETLK  := 13;
F_SETLKW := 14;

F_SETOWN_EX := 15;
F_GETOWN_EX := 16;

F_GETOWNER_UIDS := 17;


fn open(filename:string, flags:int, mode:int):int {
    fd := syscall.syscall3(syscall.sys_open as ptr, filename.bytes as ptr, (flags|os.O_CLOEXEC) as ptr, mode as ptr) as int; 
    if (fd < 0 || flags & os.O_CLOEXEC) {
        syscall.syscall3(syscall.sys_fcntl as ptr, fd as ptr, os.F_SETFD as ptr, os.FD_CLOEXEC as ptr);
    }
    return fd;
}

fn write(fd:int, s:string) {
    syscall.write(fd, s, s.length);
}

fn close(fd:int) {
    syscall.syscall1(syscall.sys_close as ptr, fd as ptr);
}
