fn test_early_return(branch: bool) -> int {
    set := fn(r: &string, to: string) {
        *r = to;
    };

    x: string;

    if true {
        defer assert(x == "");
    }

    if branch {
        defer assert(branch);
        return 0;
    }
    defer assert(x == "a");
    defer set(&x, "a");

    defer assert(x == "c");
    x = "c";

    while true {
        if x == "c" {
            break; // don't run any defers
        }
        defer assert(false);
    }

    return 0;
}

fn main() -> int {
    test_early_return(true);
    test_early_return(false);

    x := 12;

    add_13 := fn(x: &int) {
        *x += 13;
    };

    defer assert(x == 14);
    {
        defer assert(x == 12);
    }
    defer add_13(&x);
    defer assert(x == 1);

    {
        last: int;
        i: int;

        test := fn(curr: &int, last: &int) {
            assert(*curr == *last + 1);
            *last = *curr;
        };

        while i < x {
            i += 1;
            defer test(&i, &last);
        }

        s := "test";

        defer assert(s == "testing"[:4]);
    }

    x = 1;
    return 0;
}
