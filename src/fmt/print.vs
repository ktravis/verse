#import "os"

fn expand_number_type(p: ptr, nt: &NumType) -> u64 {
    if nt.size_in_bytes == 8 {
        return *(p as &u64);
    } else if nt.size_in_bytes == 4 {
        if nt.is_signed {
        // TODO: wtf
            x := *(p as &s32);
            return *((&x as ptr) as &u64);
        }
        x := *(p as &u32);
        return x as u64;
    } else if nt.size_in_bytes == 2 {
        if nt.is_signed {
            x := *(p as &s16);
            return x as u64;
        }
        x := *(p as &u16);
        return x as u64;
    } else {
        if nt.is_signed {
            x := *(p as &s8);
            return x as u64;
        }
        x := *(p as &u8);
        return x as u64;
    }
    return 777;
}

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
    mod_last: bool;
    for c in fmt {
        if c == "v" {
            if mod_last {
                count += 1;
            }
            mod_last = false;
        } else if c == "%" {
            mod_last = !mod_last;
        }
    }
    return count;
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

hex_digits := []string::{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F"};

fn utoa_hex(u: uint) -> string {
    out := "";
    while u > 0 {
        out = hex_digits[u % 16] + out;
        u /= 16;
    }
    while out.length < 8 {
        out = "0" + out;
    }
    return "0x" + out;
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
        s: string;
        if at.owned {
            s += "'";
        }
        s += "[";
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
    } else if bt == ENUM {
        et := a.type as &EnumType;
        x := expand_number_type(a.value_pointer, et.inner as &NumType);
        for &v, i in et.values {
            if x == *(v as &u64) {
                return et.members[i];
            }
        }
        return "? (" + utoa(x as uint) + ")";
    } else if bt == PTR {
        u := *(a.value_pointer as &uint);
        return utoa_hex(u);
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

    mod_last: bool;
    while i < fmt.length {
        c := fmt[i];
        if c == "v" {
            if mod_last {
                out += any_to_string(args[n]);
                n += 1;
            }
            mod_last = false;
        } else if c == "%" {
            if mod_last {
                out += "%";
            }
            mod_last = !mod_last;
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
