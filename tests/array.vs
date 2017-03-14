fn print_array(a:[]int) {
    i:int;
    while i < a.length {
        println("a[" + itoa(i) + "] = " + itoa(a[i]));
        i = i + 1;
    }
}

fn test_array_copy() {
    x:[3]int;
    y:[]int = x;

    do_stuff(x);

    // z will be a copy of x!
    z:[-]int = x;
    assert(y[1] == z[1]);
    x[0] = 100;
    // z is not modified
    assert(z[0] == 0);
    assert(x[0] == 100);
    assert(y[0] == x[0]);
}

fn test_array_of_strings() {
    x:[10]string;
    x[0] = "test";
    x[1] = "one";
    x[2] = "two";

    fn (a:[]string) {
        for s in a {
            println(s);
        }
        /*i:int;*/
        /*while i < a.length {*/
            /*println(a[i]); */
            /*i = i + 1;*/
        /*}*/
    }(x[:3]);
}

fn test_array_with_struct() {
    s := (fn () -> struct{arr:[5]int;} {
        s:struct{
            arr:[5]int;
        };
        a:[5]int;
        i:int;
        while i < a.length {
            a[i] = i;
            i = i + 1;
        }

        // `a` will be out of scope when we return,
        //  but is copied to `s` and returned
        s.arr = a;
        return s;
    }());

    i:int = 0;
    while i < s.arr.length {
        assert(i == s.arr[i]);
        i = i + 1;
    }
}

fn do_stuff(a:[]int) -> []int {
    i:int;
    while i < a.length {
        a[i] = a[i] * 2;
        i = i + 1;
    }
    return a;
}

fn main() -> int {
    /*arr := [-]int::{1, 21, 34};*/

    /*assert(arr[1] == 21);*/

    x:[3]int;
    i:int;
    /*while i < x.length {*/
        /*[>println("x[" + itoa(i) + "] = " + itoa(x[i]));<]*/
        /*x[i] = i;*/
        /*i = i + 1;*/
    /*}*/
    /*println("Array x:");*/
    /*print_array(do_stuff(x));*/
    /*println("Array x[1:3]:");*/
    /*print_array(x[1:3]);*/
    /*y:[]int = x;*/
    /*do_stuff(x);*/
    /*println("Array y:");*/
    /*print_array(y);*/
    /*i = 0;*/

    /*x = [-]int::{1, 2, 3};*/
    /*assert(x[2] == 3);*/

    /*test_array_copy();*/
    /*test_array_with_struct();*/
    test_array_of_strings();

    // multi-dimensional arrays?
    m:[3][3]int;
    while i < m.length {
        j:int;
        m[i] = x;
        while j < m[i].length {
            println("UM " + itoa(i) + ", " + itoa(j) + ": " + itoa(m[i][j]));
            j = j + 1;
        }
        i = i + 1;
    }
    return 0;
}
