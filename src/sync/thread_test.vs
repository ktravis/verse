#import "fmt"
#import "sync"
#import "time"

m:sync.Mutex;

fn func1() {
    sync.lock(&m);
    fmt.printf("func1(): Holy\n");
    sync.unlock(&m);
}

fn func2() {
    sync.lock(&m);
    fmt.printf("func2(): Shit\n");
    sync.unlock(&m);
}

fn testThreads() {
    t1 := sync.threadCreate(func1);
    t2 := sync.threadCreate(func2);
    // now we "join" ...
    time.sleep(5);
}

fn main():int {
    testThreads();
    return 0;
}
