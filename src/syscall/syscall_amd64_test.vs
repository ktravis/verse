#import "syscall";

fn main() -> int {
    syscall.write(1, "Hello\n", 6);
    return 0;
}
