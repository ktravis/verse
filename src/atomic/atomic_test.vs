#import "atomic"
#import "fmt"

fn testCompareAndSwap() {
    p:int = 2;
    t:int = 2;
    s:int = 4;
    r := atomic.compareAndSwapInt32(&p, t, s);
    fmt.printf("Compared %v to %v and replaced with %v. Variable passed in by reference is now %v.\n", r, t, s, p);
}

fn testSwap64() {
    p:s64 = 3;
    r := atomic.swapInt64(&p, 5 as s64);
    fmt.printf("Swapped %v with %v. Variable passed in by reference is now %v\n", r, 5, p);
    r = atomic.swapInt64(&p, 3 as s64);
    fmt.printf("Swapped %v with %v. Variable passed in by reference is now %v\n", r, 3, p);
}

fn main() -> int {
    testCompareAndSwap();
    testSwap64();
    return 0;
}
