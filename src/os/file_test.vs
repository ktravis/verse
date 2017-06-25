#import "os"
#import "fmt"

fn create() {
    f := os.open("plz.txt", os.O_CREAT|os.O_WRONLY|os.O_TRUNC, 0o755);
    os.write(f, "Of course this works");
    os.close(f);
}

fn remove() {
    r := os.remove("plz.txt");
    if (r != 0) {
        fmt.printf("remove plz.txt failed with error code: %\n", r);
    }
}

fn main() -> int {
    create();
    remove();
    return 0;
}
