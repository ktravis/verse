extern fn print_str(string):void;
extern fn itoa(int):string;
extern fn assert(bool);
extern fn validptr(ptr):bool;

/* block comments ?
 * */
/* nest/*
 * ed*/*/

struct wut {
    x:int;
    y:string;
    z:fn(int,int):int;
}
fn check(x:wut) {
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
    x.y = x.y + "lol";
    x.x = x.x + 1;
    print_str("within check: " + x.y + " " + itoa(x.x) + "\n");
}

fn iter_fib(n:int):int {
    last:int = 0;
    i:int = 1;
    tmp:int = 0;
    while n > 0 {
        tmp = i;
        i = i + last;
        last = tmp;
        n = n - 1;
    }
    return i;
}

fn main():int {
    b:wut;
    b.x = 1;
    b.y = "test";
    b.z = fn (a:int,b:int):int {
        return a*b;
    };
    a:^string = ^b.y;
    c:^int = ^b.x;
    assert(b.y == @a);
    assert(b.x == @c);
    check(b);
    assert(b.y == @a);
    assert(b.x == @c);
    assert(b.z(1,2) == 2);
    wee:bool = fn ():bool {
        print_str("weeee\n");
        return true;
    }();
    assert(wee);
    assert(!(fn ():bool {
        return false;
    }()));
    p:ptr;
    assert(!validptr(p));
    p = ^b;
    assert(validptr(p));
    assert(validptr(^b.x));
    print_str("Success!\n");
    return @c;
}
