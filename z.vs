extern fn print_str(string):void;
extern fn itoa(int):string;

struct wut {
    x:int;
    y:string;
}
fn check(x:wut):void {
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
    x.y = x.y + "lol"; // there is a leak here, because _fn_check doesn't know that x.y is initialized
    x.x = x.x + 1;
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
}

fn main():int {
    b:wut;
    b.x = 1;
    b.y = "test";
    a:^string = ^b.y;
    c:^int = ^b.x;
    print_str("outer: " + b.y + "\n");
    check(b);
    print_str("outer: " + b.y + "\n");
    print_str("outer: " + @a + "\n");
    return @c;
}
