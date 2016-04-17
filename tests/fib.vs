fn fib(n:int):int {
    if n < 2 {
        return 1;
    }
    return fib(n-1) + fib(n-2);
}

fn iter_fib(n:int):int {
    last:int = 0;
    i:int = 1;
    tmp:int = 0;
    while n > 0 {
        tmp = i;
        i = i + last;
        last = tmp;
        n = n - 1;
    }
    return i;
}

fn main():int {
    assert(fib(8) == 34);
    assert(iter_fib(9) == 55);
    return 0;
}
