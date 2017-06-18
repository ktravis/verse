#import "fmt"
#import "os"
#import "sync"
#import "sync/thread"
#import "syscall"
#import "time"

m:sync.Mutex;

fn testThreads() {
    t1 := thread.New(fn() {
        sync.lock(&m);
        fmt.printf("thread-1\n");
        sync.unlock(&m);
        syscall.syscall1(syscall.sys_exit, 0);
    });
    t2 := thread.New(fn() {
        sync.lock(&m);
        fmt.printf("thread-2\n");
        sync.unlock(&m);
        syscall.syscall1(syscall.sys_exit, 0);
    });
    // now we "join" ...
    time.sleep(1);

    os.exit(0);
}

fn main() -> int {
    testThreads();
    return 0;
}
