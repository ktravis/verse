#import "syscall"

SOCK_STREAM:s32 = 1;
SOCK_DGRAM:s32  = 2;

SOCK_CLOEXEC:s32 = 524288; // octal 02000000
SOCK_NONBLOCK:s32 = 2048;  // octal 04000

// family
AF_INET:u16 = 2;

// proto
IPPROTO_TCP:s32 = 6;
