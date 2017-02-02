#import "syscall"


fn exit(code:int) {
    syscall.exit_group(code);
    while true {
        syscall.exit(code);
    }
}
