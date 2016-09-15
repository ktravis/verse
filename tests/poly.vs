fn doit(x:$T, y:T) {
    println("ayyyy... lmao");
    a := #typeof(x);
    if a == #type string {
        println("itsa strings");
    } else {
        println("itsa idk");
    }
}

fn main():int {
    /*(fn(x:$T){*/
        /*println("anyony");*/
    /*})(2);*/
    doit(1, 2);
    doit("test", "test2");
    return 0;
}
