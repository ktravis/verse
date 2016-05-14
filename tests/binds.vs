fn floop():int {
    return 2;
}
fn main():int {
    grp:auto = floop;
    a:int = grp();
    b:int = 3;
    x:auto = fn ():auto {
        c:auto = #{b};
        return fn ():string {
            return #{itoa(c) + "hiiii"};
        };
    };
    y:auto = fn ():auto {
        c:[5]int;
        c[2] = 123;
        return fn ():int {
            c:auto = #{c};
            return c[2];
        };
    };
    println("woo");
    assert(a == 2);
    println(x()());
    assert(y()() == 123);
    return 0;
}
