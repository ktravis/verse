#import "os"

fn main():int {
    f := os.open("plz.txt", os.O_CREAT|os.O_WRONLY|os.O_TRUNC, 0o755);
    os.write(f, "Of course this works");
    os.close(f);
    return 0;
}
