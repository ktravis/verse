printf "Testing compilation and execution... "

make -s test
[ $? -ne 0 ] && exit 1

test_ast() {
    x=$1
    expected=$2
    #echo "test ast '$x' => '$expected'"
    res=`echo "$x" | ./compiler -a`
    if [[ "$res" != "$expected" ]]; then
        printf "\e[0;31mTest failed (got $res and expected $expected).\e[0m\n"
        exit 1
    fi
}

test_fail() {
    x=$1
    #echo "test non-compile '$x'"
    echo "$x" | ./compiler 2> /dev/null
    if [ "$?" == 0 ]; then
        printf "\e[0;31mTest failed (to fail -- compiled successfully).\e[0m\n"
        exit 1
    fi
}

printf "Testing AST construction... "

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

printf "\e[0;32mTests passed.\e[0m\n"

printf "Testing compilation failures... "

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
