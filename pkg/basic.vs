extern fn malloc(s64):&u8;
extern fn free(&u8);

type PrintBuffer : struct {
    data: [256]u8;    
    length: int;
};
fn float_to_string(f:float):string {
    s:string = "";
    while f > 1 {
    }
    if f < 0 {
        s = "-" + s;
    }
    return s;
}
fn any_to_string(a:Any):string {
    use BaseType;

    b := a.type.base;
    if b == STRING {
        return *(a.value_pointer as &string);
    } else if b == INT {
        return itoa(*(a.value_pointer as &int));
    } else if b == FLOAT {
        return "float";
    } else if b == FN {
        return a.type.name;
    } else if b == ENUM {
        e := a.type as &EnumType;
        i := 0;
        v := *(a.value_pointer as &s64);
        while i < e.values.length {
            if e.values[i] == v {
                return e.name + "." + e.members[i];
            }
            i += 1;
        }
    } else if b == REF {
        r := a.type as &RefType;
        return "&" + r.inner.name;
    } else if b == STRUCT {
        s := a.type as &StructType;
        return a.type.name;
    }
    return a.type.name;
}
fn print(fmt:string, rest: Any...) {
    buf:PrintBuffer;
    buf.length = fmt.length;

    bc:uint;

    i:int;
    for c in fmt {
        if c == '%' {
            assert(i < rest.length);
            s := any_to_string(rest[i]);
            for sc in s {
                buf.data[bc] = sc;
                bc = bc + 1;
            }
            i += 1;
        } else {
            buf.data[bc] = c;
            bc += 1;
        }
    }
    buf.data[bc] = 0;
    print_buf(buf.data.data);
}
// TODO spread

/*fn main():int {*/
    /*print("hiiii %\n", 1);*/
    /*return 0;*/
/*}*/
