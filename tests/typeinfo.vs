fn main():int {
    t := #type BaseType as &EnumType;
    for m in t.members {
        println("Test: " + m);
    }
    return 0;
}
