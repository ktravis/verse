fn floop() -> int {
    return 2;
}
fn main() -> int {
    grp := floop;
    a:int = grp();
    b:int = 3;
    /*x := fn () -> auto {*/
        /*c := #{b};*/
        /*return fn () -> string {*/
            /*return #{itoa(c) + "hiiii"};*/
        /*};*/
    /*};*/
    /*y := fn () -> auto {*/
        /*c:[5]string;*/
        /*c[2] = "123";*/
        /*return fn () -> string {*/
            /*c := #{c};*/
            /*return c[2];*/
        /*};*/
    /*};*/
    /*println("woo");*/
    /*assert(a == 2);*/
    /*println(x()());*/
    /*assert(y()() == "123");*/
    /*// println(y()()); // TODO calling this a second time causes a problem */
    /*//                 //  because the same _cl_xx is used to hold the bound*/
    /*//                 //  variables, bindings need to be reworked*/
    return 0;
}
