// test
// assert(false);
extern fn assert(bool):void;
extern fn print_str(string):void;

x:int = 2;
x1:int = 2;
x2:string = 'hi';

fn fib(n:int):int {
    if n < 2 {
        return 1;
    }
    return fib(n-1) + fib(n-2);
}

fn println(x:string):void {
    print_str(x + "\n");
}

noodler:auto = fn():void {
    println("NOOOOOOOOOOOODLE");
    println("NOOOOOOOOOOOODLE");
    println("NOOOOOOOOOOOODLE");
};

fn main():int {
    assert(2 == 2);
    assert(1 + 2 == 3);
    assert(2 * 4 ==  16 / 2);
    assert('test' == 'te' + 'st');
    x:string;
    x = "123";
    assert(x != '1234');

    z:auto = fn ():bool {
        return true;
    };
    z2:auto = z;
    assert(z());
    assert(z2());
    if x != '123' {
        assert(false);
    }
    y:string = '12';
    if x == y + '3' {
        assert(true);
    } else {
        assert(false);
    }
    assert(fib(8) == 34);
    println(x);

    blah:auto = fn(x:int, y:int):auto {
        return x * x + y * y;
    };
    assert(blah(3, 4) == 25);
    blah2:auto = fn():fn(bool):bool {
        return fn(a:bool):bool {
            return !a;
        };
    };
    blah3:auto = blah2();
    assert(blah3(false));
    assert(blah3(!true));
    assert(x1 == 2);
    x1 = x1 + 1;
    assert(x1 == 3);

    println("Tests passed.");
    return 0;
}
