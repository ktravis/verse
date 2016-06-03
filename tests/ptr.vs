type list : struct {
    name:string;
    next:&list;
};
fn prepend(n:string, to:&list):&list {
    // maybe allow 'hold l:list;' for shorthand declaration of structs
    l:list;
    m:&list = hold l;
    m.name = n;
    m.next = to;
    return m;
}
fn print_list(l:&list) {
    print_str(l.name);
    if validptr(l.next) {
        print_str(" ");
        print_list(l.next);
    }
}
fn clear_list(l:&list) {
    if validptr(l.next) {
        clear_list(l.next);
    }
    release l;
}
fn thing():&int {
    z:&int = hold 1337;
    return z;
}
fn main():int {
    x:&string = hold "test"; 
    y:&int = thing();
    print_str(itoa(*y));

    a:list;
    a.name = "last";
    b := hold a;
    b = prepend("second", b);
    b = prepend("first", b);
    print_list(b);
    clear_list(b);

    release x;
    release y;
    return 0;
}
