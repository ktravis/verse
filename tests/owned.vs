type B: struct {
    stuff: [42]int;
};

type A: struct {
    x: ' []u8;
    y: ' B;
};

fn new_A() -> 'A {
    return new A;
}

fn init_A(a: &A, n: int) {
    blah: [10]u8;
    a.x = new [n] u8;
    a.y = new B;
    /*a.x = blah[:];*/
}

fn test_A(a: &A, n: int) {
    assert(a.x.length == n);

    init_A(a, 2);
    assert(a.x.length == 2);
    a.y.stuff[n-1] = 26;
    assert(a.y.stuff[n-1] == 26);

    a.y = new B;

    assert(a.y.stuff[13] == 0);
    assert(a.y.stuff.length == 42);
}

fn test_return_owned_ref() {
    n := 23;

    a := new_A();
    init_A(a, n);
    test_A(a, n);
}

fn test_new_struct() {
    y := new B;
    y.stuff[12] = 24;

    assert(y.stuff[12] == 24);
}

fn main() -> int {
    test_return_owned_ref();
    test_new_struct();

    arr: '[]string;

    {
        n := 12;
        arr = new [n] string;
        arr[2] = "18";
    }

    assert(arr.length == 12);
    assert(arr[2] == "18");

    // TODO: this is not counting as "owned"
    /*arr: '[]string = new [n] string;*/

    b := arr;

    println("Tests passed.");

    return 0;
}
