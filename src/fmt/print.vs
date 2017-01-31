#include "../syscall/syscall_amd64.vs"

// Types didn't appear in scope so had to declare again here
enum Types:u8 {
    INTEGER=0,
    CHAR,
    STRING,
    FLOAT,
    BOOL,
    STRUCT_LIT,
    ENUM_LIT,
};


fn printf(fmt:string, args: Any...) {
    use Types;
    for val in args {
        // Type appears to be wrong?
        println("Type: " + itoa(val.type.base as int));
        if val.type.base == STRING {
            write(1, *(val.value_pointer as &string), 5);
        } else {
            write(1, "Naw, man\n", 9);
        }
    }
}

fn gprintf(fmt:string, args: $T...) {
    for val in args {
        a := #typeof(val);
        if a == #type string {
            write(1, val, val.length);
        } else {
            write(1, "Naw, man\n", 9);
        }
    }
}
