#import "syscall"

fn formatArgs(c:u8, val: Any) {
    if c == "s"[0] {
        s := *(val.value_pointer as &string);
        syscall.write(1, s, s.length);
        
    } else if c == "d"[0] {
        s := itoa(*(val.value_pointer as &int));
        syscall.write(1, s, s.length+4);
        
    }
    //switch (c) {
        //case "s":
            //return doThang(c);
        //case "d":
            //return doIntThang(c);
    //}
}

fn checkArgs(fmt:string, len:int) -> bool {
    i := 0;
    j := 0;
    while i < fmt.length {
        if fmt[i] == "%"[0] {
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

fn printf(fmt:string, args:Any...) {
    if checkArgs(fmt, args.length) {
        err := "Invalid format string - argument list whatever\n";
        syscall.write(1,  err, err.length);
        return;
    }
    i := 0;
    j := 0;
    while i < fmt.length {
        if fmt[i] == "%"[0] {
            //i = i + 1;
            i += 1;
            formatArgs(fmt[i], args[j]);
            j = j + 1;
        } else {
            syscall.write(1, fmt[i:1], 1);
        }
        i += 1;
    }
}
