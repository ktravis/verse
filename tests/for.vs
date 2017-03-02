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
    return 0;
}
