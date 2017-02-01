#import "syscall"

fn printf(fmt:string, args: Any...) {
    use BaseType;
    for val in args {
        if val.type.base == STRING {
            s := *(val.value_pointer as &string);
            syscall.write(1, s, s.length);
        } else {
            syscall.write(1, "Naw, man\n", 9);
        }
    }
}

fn gprintf(fmt:string, args: $T...) {
    for val in args {
        a := #typeof(val);
        if a == #type string {
            syscall.write(1, val, val.length);
        } else {
            syscall.write(1, "Naw, man\n", 9);
        }
    }
}
