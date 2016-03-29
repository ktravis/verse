extern fn print_str(string):void;
extern fn itoa(int):string;
extern fn assert(bool):void;

struct wut {
    x:int;
    y:string;
}
fn check(x:wut):void {
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
    x.y = x.y + "lol";
    x.x = x.x + 1;
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
}

fn main():int {
    b:wut;
    b.x = 1;
    b.y = "test";
    a:^string = ^b.y;
    c:^int = ^b.x;
    assert(b.y == @a);
    assert(b.x == @c);
    check(b);
    assert(b.y == @a);
    assert(b.x == @c);
    print_str("Success!\n");
    return @c;
}
