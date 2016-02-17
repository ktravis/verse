// test
// assert(false);
assert(2 == 2);
assert(1 + 2 == 3);
assert(2 * 4 ==  16 / 2);
assert('test' == 'te' + 'st');
x:string;
x = "123";
assert(x != '1234');

if x != '123' {
    assert(false);
}
y:string = '12';
if x == y + '3' {
    assert(true);
} else {
    assert(false);
}
print_str("\e[0;32mTests passed.\e[0m\n");
