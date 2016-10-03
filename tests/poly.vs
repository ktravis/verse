fn doit(x:$T, y:T):T {
    println("ayyyy... lmao");
    a := #typeof(x);
    if a == #type string {
        println("itsa strings");
    } else {
        println("itsa idk");
    }
    return x + y;
}

fn sum(x:[]$T):T {
    println("ayyyy lol");
    s:T;
    for a in x {
        s = s + a;
    }
    return s;
}

fn wow(f: fn($T), stuff: []T) {
    for x in stuff {
        println("stuff!");
        f(x);
    }
}

fn main():int {
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
    wow(fn(a:int) {
        a *= 2;
    }, zztop);

    return 0;
}
