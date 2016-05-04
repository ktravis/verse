type blah: string;
type thing : struct {
    a: int;
    b: bool;
};

fn grp(x:string) {
    println(x);
}

fn floop(x:thing) {
    println("x.a = " + itoa(x.a));
    if x.b {
        println("x.b = true");
    } else {
        println("x.b = false");
    }
}

fn main():int {
    // cast
    x:blah = "test" :: blah;
    // implicit cast
    y:blah = "test2";
    // fail
    // z:blah = "test3" :: string;
    grp(x::string);
    grp("heyyyy lol");

    not_a_thing:struct{
        a:int;
        b:bool;
    };
    floop(not_a_thing::thing);
    return 0;
}
