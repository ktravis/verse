#import "fmt"

type derp : struct(T,V) {
    n:T;
    a:[10]T;
    vvv:&V;
};

fn test(a:derp(s64,$V)) {
    a.a[0] = 12;
}

fn test_non_poly_use(use arg:derp(string,float)) -> string {
    return n;
}

fn test_non_poly(arg:derp(string,float)) -> string {
    return arg.a[1];
}

type Buf: struct(T){
    data:  [256]T;
    count: uint;
};

fn add_to_buf(b: &Buf($T), a: T) -> uint {
    b.data[b.count] = a;
    b.count += 1;
    return b.count;
}

type Wrapper: struct(T){
    inner: T;
};

// TODO: return type doesn't seem to work correctly for poly
/*fn var_add(args: Wrapper($T)...) -> T {*/
fn var_add(args: Wrapper(int)...) -> int {
    out: int;
    for a in args {
        out += a.inner;
    }
    return out;
}

fn concat(args: []Wrapper(string)) -> string {
    out: string;
    for a in args {
        out += a.inner;
    }
    return out;
}

fn main() -> int {
    x:derp(s64,string);
    x.a[0] = 4123214;
    x.n = 12;
    s:string = "Testing";
    x.vvv = &s;

    assert(*x.vvv == s);
    assert(x.a[0] == 4123214);
    assert(x.a.length == 10);
    assert(x.n == 12);

    y:derp(s64,string);
    y.n = 42;

    test(y);

    z:derp(s64,int);
    z.n = 42;
    assert(z.n == 42);

    z.a[0] = 32;
    test(z);
    assert(z.a[0] == 32);

    /*test_non_poly(z);*/
    ss: derp(string,float);
    ss.a[1] = "TESTING";
    assert(test_non_poly(ss) == "TESTING");

    s2 := ss;
    s2.n = "main test";
    s2.a[1] = "TEST2";
    assert(test_non_poly_use(s2) == "main test");
    assert(test_non_poly(s2) == "TEST2");

    buf:Buf(string);

    add_to_buf(&buf, "one");
    add_to_buf(&buf, "two");
    add_to_buf(&buf, "three");

    assert(buf.count == 3);
    assert(buf.data[1] == "two");

    /*x1:Wrapper<int>;*/
    /*x1.inner = 123;*/
    x1 := Wrapper(int)::{123};
    x2: Wrapper(int);
    x2.inner = 345;
    x3: Wrapper(int) = Wrapper(int)::{
        inner = 567,
    };

    assert(var_add(x1, x2, x3) == 1035);

    again := []Wrapper(string)::{
        Wrapper(string)::{"test"},
        Wrapper(string)::{"me"},
    };

    assert(concat(again) == "testme");

    println("Tests passed.");

    return 0;
}
