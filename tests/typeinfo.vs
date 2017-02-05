fn main() -> int {
    t := #type BaseType as &EnumType;
    for m in t.members {
        println("Test: " + m);
    }
    assert(t.members[5] == "STRING");
    return 0;
}
