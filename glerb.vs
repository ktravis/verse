fn main():int {
    a:int = 2;
    x:auto = fn ():int {
        return #{a};
    };
    return x();
}
