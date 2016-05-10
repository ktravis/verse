fn print_array(a:[]int) {
    i:int;
    while i < a.length {
        println("a[" + itoa(i) + "] = " + itoa(a[i]));
        i = i + 1;
    }
}
fn do_stuff(a:[]int):[]int {
    i:int;
    while i < a.length {
        a[i] = a[i] * 2;
        i = i + 1;
    }
    return a;
}
fn main():int {
    x:[3]int;
    i:int;
    while i < x.length {
        /*println("x[" + itoa(i) + "] = " + itoa(x[i]));*/
        x[i] = i;
        i = i + 1;
    }
    println("Array x:");
    print_array(do_stuff(x));
    /*println("Array x[1:2]:");*/
    /*print_array(x[1:2]);*/
    y:[]int = x;
    do_stuff(x);
    println("Array y:");
    print_array(y);
    i = 0;


    // multi-dimensional arrays?
    m:[3][3]int;
    while i < m.length {
        j:int;
        while j < m[i].length {
            println("UM " + itoa(i) + ", " + itoa(j));
            j = j + 1;
        }
        i = i + 1;
    }
    return 0;
}
