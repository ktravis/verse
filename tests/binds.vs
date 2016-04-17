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
    println("woo");
    assert(a == 2);
    println(x()());
    return 0;
}
