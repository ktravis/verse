#import "fmt"

type Vec3: struct{
    x: int;
    y: int;
    z: int;
}

fn reset(v: &Vec3) {
    v.reset();
}

impl Vec3 {
    fn reset(v: &Vec3) {
        *v = Vec3::{};
    }
    fn length(use v: Vec3) -> int {
        return x*x + y*y + z*z;
    }
}

impl Guy {
    fn move(use g: &Guy, dx: int, dy: int, dz: int) {
        pos.x += dx;
        pos.y += dy;
        pos.z += dz;
    }

    fn say_hi(g: Guy) -> string {
        println("hi it's " + g.name);
        return g.name;
    }

    fn greet(g: Guy, x: $X) -> string {
        return fmt.sprintf("% greets you, %!", g.name, x);
    }
}

type Guy: struct{
    name: string;
    pos:  Vec3;
}

type Thing: struct(T) {
    name: string;
    stuff: T;
}

impl Thing {
    fn string(t: Thing($T)) -> string {
        return fmt.sprintf("thing(%, %)", t.name, t.stuff);
    }
    fn test(t: Thing($T)) -> bool {
        return true;
    }
}

impl Thing(int) {
    fn test(t: Thing(int)) -> bool {
        return false;
    }
}

fn main() -> int {
    g := Guy::{
        name = "alf",
        pos  = Vec3::{1, 2, 3},
    };

    assert(g.name == g.say_hi());

    fmt.printf("%: % (len %)\n", g.name, g.pos, g.pos.length());
    assert(g.pos.length() == 14);

    g.pos.reset();
    fmt.printf("%: % (len %)\n", g.name, g.pos, g.pos.length());
    {
        use g.pos;
        assert(x == 0);
        assert(y == 0);
        assert(z == 0);
    }

    g.move(10, 5, 0);
    fmt.printf("%: % (len %)\n", g.name, g.pos, g.pos.length());
    {
        use g;
        assert(pos.length() == 125);

        reset(&g.pos);
        assert(pos.length() == 0);
    }

    assert((Thing(int)::{"mork", 2}).string() == "thing(mork, 2)");
    assert((Thing(Vec3)::{"mindy", Vec3::{0, 1, 2}}).string() == "thing(mindy, Vec3::{x: int = 0, y: int = 1, z: int = 2})");

    g.greet("Steve");
    assert((Guy::{name = "Steve"}).greet(2) == "Steve greets you, 2!");

    u: Thing(Guy);
    t: Thing(int);

    fmt.printf("test: %\n", u.test());
    assert(u.test());
    assert(!t.test());

    return 0;
}
