#import "fmt"
#import "time"

fn main() -> int {
    tm := time.time();
    fmt.printf("Did it work? %d %d\n", tm.tv_sec, tm.tv_nsec);
    fmt.printf("Did sleep????\n");
    time.usleep(500000);
    fmt.printf("It dooooo\n");
    return 0;
}
