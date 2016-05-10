type vec3: struct {
    x:int;
    y:int;
    z:int;
};

v1:vec3 = vec3{x = 1, z = 2};

fn doathing(a:^vec3) {
    a.x = a.x + a.y * a.z;
}

fn otherthing(b:vec3):vec3 {
    tmp:int = b.x;
    b.x = b.y;
    b.y = tmp;
    return b;
}

fn hold_struct(): ^vec3 {
    return hold vec3{x=1,y=2,z=3};
}

fn main():int {
    a:auto = hold vec3 {
        x = 1,
        y = 2,
        z = 3
    };
    doathing(a);
    assert(v1.z == 2);
    print_str("test: " + itoa(a.x) + " " + itoa(a.y) + " " + itoa(a.z));
    print_str("\n-------\n");
    assert(a.x == 7);
    release a;
    assert((vec3{}).x == 0);
    assert(otherthing(vec3{x=1,y=2,z=3}).x == 2);
    x:struct { b: int; };
    x.b = 1;
    y:^struct{b:int;} = hold x;
    assert(1 == y.b);
    release y;
    a1:auto = hold_struct();
    println("held: x = " + itoa(a1.x) + ", y = " + itoa(a1.y) + ", z = " + itoa(a1.z));
    release a1;
    return 0;
}
