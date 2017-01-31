type vec3: struct {
    x:int;
    y:int;
    z:int;
};

v1:vec3 = vec3::{x = 1, z = 2};

fn check_use(a:vec3, use b:vec3) {
    l:struct{
      r:int;
      n:float;
    };
    use l;
    b.x = 12;
    l.r = 24 / 2;
    assert(x == r);
}

fn check_use_ptr(use a:&vec3) {
    x = x + y * z;
}

type Marker : struct {
    pos: vec3;
    dir: vec3;
};

fn check_use_depth(use m:Marker) {
    use dir;
    x = 2;
    assert(x == (m.pos.x + 2));
}

fn otherthing(use b:vec3):vec3 {
    tmp:int = x;
    x = y;
    y = tmp;
    return b;
}

fn main():int {
    assert(v1.z == 2);

    a := vec3::{
        x = 1,
        y = 2,
        z = 3
    };

    check_use(a, v1);

    check_use_ptr(&a);

    check_use_depth(Marker::{});

    print_str("test: " + itoa(a.x) + " " + itoa(a.y) + " " + itoa(a.z));

    print_str("\n-------\n");

    assert(a.x == 7);

    // TODO: remove parens and have parse bug, priority is off?
    assert((vec3::{}).x == 0);

    assert(otherthing(vec3::{x=1,y=2,z=3}).x == 2);

    x:struct { b: int; };

    x.b = 1;

    empty:vec3 = vec3::{};
    assert(empty.x == 0);

    return 0;
}
