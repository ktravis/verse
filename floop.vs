extern fn print_str(string);
extern fn itoa(int):string;
extern fn validptr(ptr):bool;

struct list {
    name:string;
    next:^list;
}
fn prepend(n:string, to:^list):^list {
    hold l:list;
    l.name = n;
    l.next = to;
    return ^l;
}
fn print_list(l:^list) {
    print_str(l.name);
    if validptr(l.next) {
        print_str(" ");
        print_list(l.next);
    }
}
fn clear_list(l:^list) {
    if validptr(l.next) {
        clear_list(l.next);
    }
    release l;
}
fn thing():^int {
    hold z:int = 1337;
    return ^z;
}
fn main():int {
    hold x:string = "test"; 
    y:^int = thing();
    print_str(itoa(@y));

    hold a:list;
    a.name = "last";
    b:auto = ^a;
    b = prepend("second", b);
    b = prepend("first", b);
    print_list(b);
    clear_list(b);

    release x;
    release y;
    return 0;
}
