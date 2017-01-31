// TODO: fix this (absolute package path from src root)
#import "../src/fmt"

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

fn test3() {
    fmt.gprintf("%s", "Hello", "There");
}

fn test4() {
    a:string = "Hello";
    b:string = "There";
    fmt.gprintf("%s", a, b);
}

fn test5() {
    // causes free to be emitted for both 'scopes' of conditional
    /*if 1 == 1 {
        s:string = "Rolo";
    } else {
        printf("%s", "Polo");
    }*/
}

fn main():int {
    //test1();
    //test2();
    test3();
    //test4();
    return 0;
}
