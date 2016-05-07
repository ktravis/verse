fn main():int {
    x:[3]int;
    y:[]int = x;
    println(itoa(y.length));
    i:int;
    while i < x.length {
        v:int = @((x.data::int64 + (i*4))::^int);
        println("x[" + itoa(i) + "] = " + itoa(v));
        i = i + 1;
    }
    z:^int = y.data;
    /*@z = 2;*/
    i = 0;
    while i < y.length {
        v:int = @((y.data::int64 + (i*4))::^int);
        println("y[" + itoa(i) + "] = " + itoa(v));
        i = i + 1;
    }
    return 0;
}
