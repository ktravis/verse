extern fn print_str(string):void;
extern fn itoa(int):string;

struct wut {
    x:int;
    y:string;
}
struct uhhh {
    z:wut;
}

fn check(x:wut):void {
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
    x.y = x.y + "lol"; // there is a leak here, because _fn_check doesn't know that x.y is initialized
    x.x = x.x + 1;
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
}

fn main():int {
    b:wut;
    a:string = "testtttt";
    b.x = 1;
    b.y = "test";
    c:uhhh;
    print_str(a + "\n");
    print_str("outside check: " + b.y + " " + itoa(b.x) + "\n");
    check(b);
    print_str("outside check: " + b.y + " " + itoa(b.x) + "\n");
    c.z.y = ", world!\n";
    print_str("Hello" + c.z.y);
    return 0;
}
