fn variadic(a:string, rest:Any...) -> Any {
    if rest.length > 2 {
        return rest[2];
    }
    return a;
}

fn main() -> int {
    variadic("testing");
    assert(*(variadic("testing", 2, 2.3, "hello").value_pointer as &string) == "hello");
    return 0;
}
