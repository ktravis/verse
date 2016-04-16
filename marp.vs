struct vec3 {
    x:int;
    y:int;
    z:int;
}

v1:vec3 = vec3{x = 1, z = 2};

fn doathing(a:^vec3) {
    a.x = a.x + a.y * a.z;
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
    return 1;
}
