// made this an include for now
#include "syscall_amd64.vs";

fn main():int {
    write(1,  "Hello\n", 6);
    return 0;
}
