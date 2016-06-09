fn main():int {
    x:int = 2;
    assert(x - 3 == -1);

    y:float = 1.0 / 2 + 3;
    assert(y == 3.5);
    assert(fn (x:float):bool {
        return (true || false) && true;
    }(2));

    assert(0x1702 == 5890);

    z:uint = 200 - 200;
    return 0;
}
