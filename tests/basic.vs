// test
// assert(false);
/* block comments ?
 * */
/* nest/*
 * ed*/*/

x:int = 2;
x1:int = 2;
x2:string = 'hi';

fn main():int {
    assert(2 == 2);
    assert(1 + 2 == 3);
    assert(2 * 4 ==  16 / 2);
    assert('test' == 'te' + 'st');
    x:string;
    x = "123";
    assert(x != '1234');

    z := fn ():bool {
        return true;
    };
    z2 := z;
    assert(z());
    assert(z2());
    if x != '123' {
        assert(false);
    }
    y:string = '12';
    if x == y + '3' {
        assert(true);
    } else {
        assert(false);
    }
    println(x);

    blah := fn(x:int, y:int):auto {
        return x * x + y * y;
    };
    assert(blah(3, 4) == 25);
    blah2 := fn():fn(bool):bool {
        return fn(a:bool):bool {
            return !a;
        };
    };
    blah3 := blah2();
    assert(blah3(false));
    assert(blah3(!true));
    assert(x1 == 2);
    x1 = x1 + 1;
    assert(x1 == 3);

    if x1 == 2 {
        assert(false);
    } else if x1 == 4 {
        assert(false);
    } else if x1 == 3 {
        assert(true);
    } else {
        assert(false);
    }

    println("Tests passed.");

    return 0;
}
