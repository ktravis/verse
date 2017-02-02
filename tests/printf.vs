#import "fmt"

fn test1() {
    fmt.printf("%s", "Hello", "There");
}

fn test2() {
    a:string = "Hello";
    b:string = "There";
    // causes segfault if uncommented, segfault
    // happens whether code is reached or not
    /*printf("%s", a, b);*/
}

fn main():int {
    test1();
    //test2();
    return 0;
}
