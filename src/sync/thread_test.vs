#import "fmt"
#import "os"
#import "sync"
#import "syscall"
#import "time"

m:sync.Mutex;

fn func1() {
    sync.lock(&m);
    fmt.printf("func1(): Nice\n");
    sync.unlock(&m);
    // TODO: sys_exit is required for the thread to clean up but
    // clone function appears to be calling sys_exit in some cases.
    // should probably get a better handle on what it is doing.
    syscall.syscall1(syscall.sys_exit, 0);
}

fn func2() {
    sync.lock(&m);
    fmt.printf("func2(): Dawg\n");
    sync.unlock(&m);
    // TODO: sys_exit is required for the thread to clean up but
    // clone function appears to be calling sys_exit in some cases.
    // should probably get a better handle on what it is doing.
    syscall.syscall1(syscall.sys_exit, 0);
}

fn testThreads() {
    t1 := sync.threadCreate(func1);
    t2 := sync.threadCreate(func2);
    // now we "join" ...
    time.sleep(1);

    os.exit(0);
}

fn main() -> int {
    testThreads();
    return 0;
}
