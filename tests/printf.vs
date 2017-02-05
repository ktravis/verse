#import "fmt"
#import "os"

fn test1() {
    fmt.printf("%s, %s\n", "Hi", "Planet!");
}

fn test6() {
    fmt.printf("%s, %s %d\n", "Hi", "Planet!");
}

fn test2() {
    a:string = "Hello";
    b:string = "There";
    // causes segfault if uncommented, segfault
    // happens whether code is reached or not
    // fmt.printf("%s", a, b);
}

fn main() -> int {
    test1();
    test6();
    //test2();
    return 0;
}
