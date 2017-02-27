#import "fmt"

type Vec3: struct{
    x: s64;
    y: s64;
    z: s64;
    extra: string;
};

fn main() -> int {
    c := "blerp"[1];
    x: u64 = 2147483648;
    fmt.printf("% %, %: %\n", "Hi", x, "Planet!", Vec3::{1, 2, 3, ""});
    /*f: float64 = 0.31415124;*/
    /*fmt.printf("um: % %\n", 0.125, f);*/
    x1 := [2]string::{"101", "202"};
    fmt.printf("static array: % %\n", x1, &x1);
    fmt.printf("array: %\n", []int::{1, 2, 3, 4, 12});
    return 0;
}
