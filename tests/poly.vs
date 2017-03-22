fn doit(x:T, y:$T) -> T {
    println("ayyyy... lmao");
    a := #typeof(x);
    if a == #type string {
        println("itsa strings");
    } else {
        println("itsa idk");
    }
    return x + y;
}

fn derf(x: $T, y: &T) {}

fn sum(x:[]$T) -> T {
    println("ayyyy lol");
    s:T;
    for a in x {
        s = s + a;
    }
    return s;
}

fn var(x:$T...) -> T {
    return x[1];
}

fn eh(x:$T, f:fn(T) -> T) -> T {
    return f(x);
}

fn wow(f: fn($R), stuff: []R) {
    for x in stuff {
        println("stuff!");
        f(x);
    }
}

fn main() -> int {
    (fn(x:$T){
        println("anyony");
    })(2);
    x:[2]int;
    x[0] = 1;
    x[1] = 12;
    println("Uh: " + itoa(sum(x)));
    doit(1, 2);
    doit("test", "test2");

    strs:[3]string;
    strs[0] = "oh";
    strs[1] = " hai ";
    strs[2] = "there";
    println(sum(strs));

    zztop:[2]int;
    zztop[0] = 3;
    zztop[1] = 84;
    wow(fn(a:int) {
        a *= 2;
        println("in wow: " + itoa(a));
    }, zztop);

    println("variadic (int): " + itoa(var(1, 2, 3)));
    println("variadic (string): " + var("Hello, ", "World"));
    anys:[2]Any;
    anys[0] = 1.2;
    anys[1] = "test, please";
    a := var(anys[0], anys[1]);
    println(*(a.value_pointer as &string));

    // TODO: bug
    /*eh(2, fn(a: int) -> int { return a; });*/

    return 0;
}
