#import "syscall"
#import "net"
#import "fmt"


type SocketAddress: struct {
    family: u16
    port: u16
    addr: u32
    zero: [8]u8
}

fn connect(fd: int, addr: &SocketAddress) -> int {
    t := #typeof(addr.addr);
    bt := t.base;
    return syscall.syscall3(syscall.sys_connect, fd, addr, 16) as int;
}

fn socket(family: u16, socketType: s32, protocol: s32) -> int {
    return syscall.syscall3(syscall.sys_socket, family, socketType, protocol) as int;
}
