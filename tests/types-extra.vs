// we can use types before they are defined within the same scope
fn test(x:blerg) {
    println("blerg");
}

// type definitions will fill in dependencies
// types can recurse
type blerg : struct {
    a:int;
    b:string;
    c:&blerg;
};

fn main() -> int {
    // even if a type resolves, if it is shadowed later in the same scope,
    // that definition will take precedence
    b:blerg;

    // use #typeof to get the &Type of a value/expression (at compile time)
    t := #typeof(b) as &StructType;

    i := 0;
    for m in t.members {
        println("member " + itoa(i) + ": " + m.name);
        i = i + 1;
    }

    // named (const) functions (only const!) can be called before they are
    // defined and will resolve correctly in the local scope only.
    test(b);

    // fall back on parent definition if func is not defined locally
    fn test(x:blerg) {
        println("thing");
    }

    // fall back on parent definition if type is not defined locally
    type blerg : struct {
        floop:float;
    };

    return 0;
}
