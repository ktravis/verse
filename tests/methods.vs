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
}

type Guy: struct{
    name: string;
    pos:  Vec3;
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

    return 0;
}
