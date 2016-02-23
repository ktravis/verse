// test
// assert(false);
assert(2 == 2);
assert(1 + 2 == 3);
assert(2 * 4 ==  16 / 2);
assert('test' == 'te' + 'st');
x:string;
x = "123";
assert(x != '1234');

if x != '123' {
    assert(false);
}
y:string = '12';
if x == y + '3' {
    assert(true);
} else {
    assert(false);
}

fn fib(n:int):int {
    if n < 2 {
        return 1;
    }
    return fib(n-1) + fib(n-2);
}

fn println(x:string):void {
    print_str(x + "\n");
}
assert(fib(8) == 34);

fn green(x:string):string {
    return "\e[0;32m"+x+"\e[0m";
}

println(green("Tests passed."));
