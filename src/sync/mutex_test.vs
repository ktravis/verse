#import "fmt"
#import "sync"

fn testLock() {
    m:sync.Mutex;
    sync.lock(&m);
    fmt.printf("Doin' lock-y stuf\n");
    sync.unlock(&m);
    fmt.printf("Unlocked successfully\n");
}

fn main():int {
    testLock();
    return 0;
}
