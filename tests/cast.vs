type blah: string;

fn grp(x:string) {
    println(x);
}

fn main():int {
    x:blah = "a" + "test" :: blah;
    grp(x::string);
    return 0;
}
