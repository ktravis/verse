fn test_by_reference() {
    arr := []s64::{0, 2, 4, 6, 8};
    for &x in arr {
        *x += 1;
    }
    for x in arr {
        assert(x % 2 == 1);
    }
}

fn test_by_reference_string() {
    arr := []string::{"a", "b", "c"};
    for &x in arr {
        *x += *x;
    }
    for x in arr {
        assert(x.length == 2 && x[0] == x[1]);
    }
}

fn main() -> int {
    arr: [10]string;

    i: int;
    while i < arr.length {
        arr[i] = itoa(i * 2);
        i += 1;
    }

    for a, i in arr {
        assert(a == itoa(i * 2));
    }

    for a, u: uint in arr[1:5] {
        if u == 0 {
            assert(a == "2");
        }
        assert(a == utoa((u + 1) * 2));
        assert(u < 5);
    }

    for x in []s64::{0, 2, 4, 6, 8} {
        assert(x % 2 == 0);
    }

    test_by_reference();
    test_by_reference_string();
    return 0;
}
