make -s compiler
[ $? -ne 0 ] && exit 1

test() {
    x=$1
    expected=$2
    echo "test '$x' => '$expected'"
    echo "$x" | ./compiler > tmp.s
    [ $? -ne 0 ] && exit 1
    gcc -o tmp.out driver.c tmp.s
    [ $? -ne 0 ] && exit 1
    res=`./tmp.out`
    if [[ "$res" != "$expected" ]]; then
        printf "\e[0;31mTest failed (got $res and expected $expected).\e[0m\n"
        exit 1
    fi
}

test_ast() {
    x=$1
    expected=$2
    echo "test ast '$x' => '$expected'"
    res=`echo "$x" | ./compiler -a`
    if [[ "$res" != "$expected" ]]; then
        printf "\e[0;31mTest failed (got $res and expected $expected).\e[0m\n"
        exit 1
    fi
}

test_fail() {
    x=$1
    echo "test non-compile '$x'"
    echo "$x" | ./compiler 2> /dev/null
    if [ "$?" == 0 ]; then
        printf "\e[0;31mTest failed (to fail -- compiled successfully).\e[0m\n"
        exit 1
    fi
}

#rm tmp.out tmp.s 2>/dev/null
#test '0;' 0
#test '42;' 42
#test 'x:string = "test";printf("%s", cstr(x));1;' test1
#test '1 + 2;' 3
#test '1 + 2 * 3;' 7
#test '(1 + 2) * 3;' 9
#test '(1 - (1 + 2)) * 3;' -6
#test '23 * 164 - 2;' 3770
#test '2 - 23 * 164 + 1;' -3769
#test '164 / 41 * 23 + 1;' 93
#test 'a:int = 1;b:int = 2;a + b;' 3
#test 'a:int=1;b:int=2;a + b;' 3
#test 'printf("%d", 64);3;' 643
#test "printf('%d', add(1, 63));3;" 643
#test 'a:int=1;printf("%d", a);b:int=2;b;' 12
#test 'add(42,18);' 60
#test 'x:int = 0;x;' 0
#test 'x:int = 1;x=2+1;x-3;' 0
#test 'x:int = 1; { x:int = 2; { x:int = 3; } } x;' 1
#test "if 1 < 2 { printf('thing'); }2;" 'thing2'
#test "if false { printf('thing'); } else { 1; }" 1
#test "a:bool = 1 + 0 > 1;a;" 0
#test "a:bool = 1 < 2;a;" 1
#test "a:bool = 3 >= 3;a;" 1
#test "a:bool = 2 <= 2;a;" 1
#test "a:bool = 2 <= 1;a;" 0
#test "a:bool = 1 != 2;a;" 1
#test "a:bool = 1 == 2;a;" 0
#test "a:bool = false == true;a;" 0
#test "a:bool = false == false;a;" 1

test_ast "0;" "{ 0 }"
test_ast "a:int;" "{ (decl a int) }"
test_ast "a:int = 1;b:int = a + 1;" "{ (decl a int 1)(decl b int (+ a 1)) }"
test_ast "1 + 2;" "{ (+ 1 2) }"
test_ast "'test';" '{ "test" }'
test_ast "1 + 2 * 3;" "{ (+ 1 (* 2 3)) }"
test_ast "(1 + 2) * 3;" "{ (* (+ 1 2) 3) }"
test_ast "2 - 23 * 164 + 1;" "{ (+ (- 2 (* 23 164)) 1) }"
test_ast "2 / 23 * 164 + 1;" "{ (+ (* (/ 2 23) 164) 1) }"
test_ast "1 + 3;test(1, 2, 3 , 4);"  "{ (+ 1 3)test(1,2,3,4) }"
test_ast "x();test( 1, 2, 3 , 4);"  "{ x()test(1,2,3,4) }"
test_ast "x(a(1,2),3);" "{ x(a(1,2),3) }"
test_ast "if 1 < 2 { printf('thing'); }" '{ (if (< 1 2) printf("thing")) }'
test_ast "if false { printf('thing'); } else { 1; }" '{ (if false printf("thing") 1) }'

test_fail "0"
test_fail ";"
test_fail "x;"
test_fail "x = 1;"
test_fail "(1 + 2;"
test_fail "x:a = 1;"
test_fail "x:int = 1;x:int = 2;" # something screwy might be going on here
test_fail "x:string = 2;"
test_fail "x:int = 2;x = 'string\";"
test_fail "{ x:int = 2;"
test_fail "{ x:int = 2; } x;"
test_fail "if { x:int = 2; } x;"
test_fail "if { x:int = 2;"
test_fail "if { x:int = 2; };"

printf "\e[0;32mTests passed.\e[0m\n"
#rm tmp.out tmp.s
