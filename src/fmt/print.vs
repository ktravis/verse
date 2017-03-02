#import "os"

fn formatArgs(c:u8, val: Any) {
    s: string;
    if c == "s" {
        s = *(val.value_pointer as &string);
    } else if c == "d" {
        s = itoa(*(val.value_pointer as &int));
    }
    os.write(os.Stdout, s);
}

fn numFormatArgs(fmt: string) -> int {
    count: int;
    last: u8;
    for c in fmt {
        if c == "%" {
            if last == "%" {
                last = 0;
                count -= 1;
                continue;
            } else {
                count += 1;
            }
        }
        last = c;
    }
    return count;
}

fn checkArgs(fmt:string, len:int) -> bool {
    i := 0;
    j := 0;
    while i < fmt.length {
        if fmt[i] == "%" {
            j += 1;
        }
        if j > len {
            break;
        }
        i += 1;
    }
    if j != len {
        return true;
    }
    return false;
}

fn uint_to_binary(x: uint, digits: uint) -> string {
    s: string;

    c: int;

    while c < digits {
        if ((1 << c) & x) != 0 {
            s = "1" + s;
        } else {
            s = "0" + s;
        }
        c += 1;
    }
    return s;
}

fn float64_to_string(f: float64) -> string {
    p := (&f as ptr) as &u64;
    sign := (*p >> 63) & 0x1;
    // TODO: this needs to be different, it is wrapping around rather than being signed
    exp := ((*p >> 52) & 0x7ff) - 1023;
    m := *p & ((1 << 52) - 1);


    s: string;

    if sign != 0 {
        s += "-";
    }

    printf("exp %\n", exp);
    whole := (1 << exp) | (m >> (52 - exp));
    s += utoa(whole as uint);

    frac := m & ((1 << (52 - exp)) - 1);
    base := (1 << (52 - exp));

    s += ".";

    c: uint;
    if frac == 0 {
        s += "0";
    } else {
        while frac != 0 && c < 15 {
            c += 1;
            frac *= 10;
            s += utoa((frac / base) as uint);
            frac %= base;
        }
    }

    return s;
}

fn float32_to_string(f: float32) -> string {
    p := (&f as ptr) as &u64;
    sign := *p >> 31 & 0x1;
    exp := ((*p >> 23) & 0xff) - 127;
    m := (*p) & ((1 << 23) - 1);
    m |= (1 << 23);

    s: string;

    if sign != 0 {
        s += "-";
    }
    s += utoa((m >> (23 - exp)) as uint);
    s += ".";

    frac := m & ((1 << (23 - exp)) - 1);
    base := (1 << (23 - exp));

    c := 0;
    if frac == 0 {
        s += "0";
    } else {
        while frac != 0 && c < 6 {
            c += 1;
            frac *= 10;
            s += utoa((frac / base) as uint);
            frac %= base;
        }
    }

    return s;
}

fn any_to_string(a: Any) -> string {
    use BaseType;
    bt := a.type.base;

    if bt == STRING {
        return *(a.value_pointer as &string);
    } else if bt == FLOAT {
        t := a.type as &NumType;
        size := t.size_in_bytes;
        if size == 8 {
            return float64_to_string(*(a.value_pointer as &float64));
        } else if size == 4 {
            return float32_to_string(*(a.value_pointer as &float32));
        }
    } else if bt == INT {
        t := a.type as &NumType;
        size := t.size_in_bytes;
        if t.is_signed {
            if size == 8 {
                return itoa(*(a.value_pointer as &int));
            } else if size == 4 {
                return itoa(*(a.value_pointer as &s32) as int);
            } else if size == 2 {
                return itoa(*(a.value_pointer as &s16) as int);
            } else if size == 1 {
                return itoa(*(a.value_pointer as &s8) as int);
            }
        } else {
            if size == 8 {
                v := *(a.value_pointer as &uint);
                return utoa(v);
            } else if size == 4 {
                return itoa(*(a.value_pointer as &u32) as int);
            } else if size == 2 {
                return itoa(*(a.value_pointer as &u16) as int);
            } else if size == 1 {
                return itoa(*(a.value_pointer as &u8) as int);
            }
        }
    } else if bt == ANY {
        return "ANY";
    } else if bt == BOOL {
        b := *(a.value_pointer as &bool);
        if b {
            return "true";
        }
        return "false";
    } else if bt == STRUCT {
        s := a.type.name;
        s += "::{";
        st := a.type as &StructType;
        i := 0;
        for member in st.members {
            if i != 0 {
                s += ", ";
            }
            s += member.name + ": " + member.type.name + " = ";
            s += any_repr(Any::{(a.value_pointer as uint + member.offset) as ptr,member.type});
            i += 1;
        }
        return s + "}";
    } else if bt == ARRAY {
        at := a.type as &ArrayType;
        s := "[";
        if at.is_static {
            s += itoa(at.size);
        }
        s += "]" + at.inner.name + "::{";

        stride := size_of_type(at.inner);
        length := at.size;
        data: uint;

        if at.is_static {
            data = a.value_pointer as uint;
        } else {
            tmp := *(a.value_pointer as &[]u8); // this type doesn't matter
            length = tmp.length;
            data = (tmp.data as ptr) as uint;
        }

        i := 0;
        while i < length {
            if i != 0 {
                s += ", ";
            }
            if at.inner == #type Any {
                s += any_repr(*(((data + i * stride) as ptr) as &Any));
            } else {
                s += any_repr(Any::{(data + i * stride) as ptr,at.inner});
            }
            i += 1;
        }
        return s + "}";
    }
    return a.type.name;
}

fn size_of_type(t: &Type) -> int {
    use BaseType;

    bt := t.base;
    if bt == STRING || bt == ANY {
        return 16;
    } else if bt == BOOL {
        return 1;
    } else if bt == VOID {
        return 0;
    } else if bt == PTR {
        return 8;
    } else if bt == INT || bt == FLOAT {
        t := t as &NumType;
        return t.size_in_bytes;
    } if bt == REF || bt == FN {
        return 8;
    } if bt == ENUM {
        et := t as &EnumType;
        return size_of_type(et.inner);
    } else if bt == STRUCT {
        st := t as &StructType;
        n := 0;
        for member in st.members {
            n += size_of_type(member.type);
        }
        return n;
    } else if bt == ARRAY {
        at := t as &ArrayType;
        if at.is_static {
            return at.size * size_of_type(at.inner);
        }
        return 16;
    }
    assert(false);
    return -1;
}

fn any_repr(a: Any) -> string {
    s := any_to_string(a);
    if a.type.base == BaseType.STRING {
        return "\"" + s + "\"";
    }
    return s;
}

fn sprintf(fmt: string, args: Any...) -> string {
    out: string;

    expected := numFormatArgs(fmt);
    if expected != args.length {
        os.write(os.Stderr, "Format string expects " + itoa(expected) + " args but received " + itoa(args.length) + ".\n");
        return out;
    }

    i := 0;
    n := 0;

    while i < fmt.length {
        c := fmt[i];
        if c == "%" {
            if i + 1 < fmt.length && fmt[i+1] == "%" {
                out += "%";
                i += 2;
                continue;
            } else {
                out += any_to_string(args[n]);
                n += 1;
            }
        } else {
            out += fmt[i:1];
        }
        i += 1;
    }
    return out;
}

fn printf(fmt:string, args:Any...) {
    os.write(os.Stdout, sprintf(fmt, args...));
}
