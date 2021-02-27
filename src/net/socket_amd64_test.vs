#import "fmt"
#import "net"
#import "os"
#import "syscall"

fn flip16u(port: u16) -> u16 {
    return (port << 8) | (port >> 8); // & 0xffff;
}

fn flip32u(n: u32) -> u32 {
    return ((n >> 24) & 0xff) | ((n << 8) & 0xff0000) | ((n >> 8) & 0xff00) | ((n << 24) & 0xff000000);
}

fn lookupHost(family: u16) -> int {
    fd := net.socket(family, net.SOCK_DGRAM|net.SOCK_CLOEXEC|net.SOCK_NONBLOCK, 0);
    if fd < 0 {
        fmt.printf("Socket failed\n");
        return 1;
    }
    return 0;
}

fn testSocket() -> int {
    a: net.SocketAddress;
    a.family = net.AF_INET;
    a.port = flip16u(8080);
    a.addr = flip32u(0x7f000001); // 127.0.0.1
    fd := net.socket(net.AF_INET, net.SOCK_STREAM, net.IPPROTO_TCP);

    // TODO(chris): for some reason had to move this above the if statements
    // because the codegen is trying to free it before it is declared:
    //   if ((_vs_14 < 0LL)) {
    //               ...
    //               free(_vs_15.bytes);
    //               return _ret;
    //           }
    //       }
    //       ...
    //   struct string_type _vs_15 = init_string("GET / HTTP/1.1\r\n\r\n", 18);
    request := "GET / HTTP/1.1\r\n\r\n";

    if fd < 0 {
        fmt.printf("Socket failed\n");
        return 1;
    }
    ret := net.connect(fd, &a);
    if ret < 0 {
        fmt.printf("Socket connect failed\n");
        return 1;
    }
    fmt.printf("connected\n");
    syscall.syscall3(syscall.sys_write, fd, request.bytes, request.length);
    fmt.printf("Sent request:\n%s\n", request);

    buf: [1024]u8;
    while true {
        n := syscall.syscall3(syscall.sys_read, fd, buf, 1024) as int;
        if n <= 0 {
            break;
        }
        syscall.syscall3(syscall.sys_write, 1, buf, n);
    }
    syscall.syscall1(syscall.sys_close, fd);
    return 0;
}

fn main() -> int {
    testSocket();
    return 0;
}
