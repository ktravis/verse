#import "os"
#import "fmt"

fn main() -> int {
    os.exit(1);
    fmt.printf("os.exit did not work\n");
    return 0;
}
