fn main() -> int {
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
