#import "fmt"

// TODO: can't return any at all, dingus! it's not safe!
fn variadic(a:int, rest:Any...) -> Any {
    if rest.length > 2 {
        return rest[2];
    }
    return a;
}

fn main() -> int {
    variadic(2, "testing");
    assert(*(variadic(0, "testing", 2.3, "hello").value_pointer as &string) == "hello");

    x := []Any::{1, 2, "test", 4};

    assert(*(variadic(0, x...).value_pointer as &string) == "test");
    assert(*(variadic(0, x[1:]...).value_pointer as &int) == 4);
    return 0;
}
