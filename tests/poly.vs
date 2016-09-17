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
    return a;
}

fn main():int {
    (fn(x:$T){
        println("anyony");
    })(2);
    x:[2]int;
    sum(x);
    doit(1, 2);
    doit("test", "test2");
    return 0;
}
