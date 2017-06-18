MAP_SHARED     := 0x01;
MAP_PRIVATE    := 0x02;
MAP_TYPE       := 0x0f;
MAP_FIXED      := 0x10;
MAP_ANON       := 0x20;
MAP_ANONYMOUS  := MAP_ANON;
MAP_NORESERVE  := 0x4000;
MAP_GROWSDOWN  := 0x0100;
MAP_DENYWRITE  := 0x0800;
MAP_EXECUTABLE := 0x1000;
MAP_LOCKED     := 0x2000;
MAP_POPULATE   := 0x8000;
MAP_NONBLOCK   := 0x10000;
MAP_STACK      := 0x20000;
MAP_HUGETLB    := 0x40000;
MAP_FILE       := 0;

PROT_NONE      := 0;
PROT_READ      := 1;
PROT_WRITE     := 2;
PROT_EXEC      := 4;
PROT_GROWSDOWN := 0x01000000;
PROT_GROWSUP   := 0x02000000;

CSIGNAL               :=  0x000000ff;
CLONE_VM              :=  0x00000100;
CLONE_FS              :=  0x00000200;
CLONE_FILES           :=  0x00000400;
CLONE_SIGHAND         :=  0x00000800;
CLONE_PTRACE          :=  0x00002000;
CLONE_VFORK           :=  0x00004000;
CLONE_PARENT          :=  0x00008000;
CLONE_THREAD          :=  0x00010000;
CLONE_NEWNS           :=  0x00020000;
CLONE_SYSVSEM         :=  0x00040000;
CLONE_SETTLS          :=  0x00080000;
CLONE_PARENT_SETTID   :=  0x00100000;
CLONE_CHILD_CLEARTID  :=  0x00200000;
CLONE_DETACHED        :=  0x00400000;
CLONE_UNTRACED        :=  0x00800000;
CLONE_CHILD_SETTID    :=  0x01000000;
CLONE_NEWCGROUP       :=  0x02000000;
CLONE_NEWUTS          :=  0x04000000;
CLONE_NEWIPC          :=  0x08000000;
CLONE_NEWUSER         :=  0x10000000;
CLONE_NEWPID          :=  0x20000000;
CLONE_NEWNET          :=  0x40000000;
CLONE_IO              :=  0x80000000;

SIGHUP    := 1;
SIGINT    := 2;
SIGQUIT   := 3;
SIGILL    := 4;
SIGTRAP   := 5;
SIGABRT   := 6;
SIGIOT    := SIGABRT;
SIGBUS    := 7;
SIGFPE    := 8;
SIGKILL   := 9;
SIGUSR1   := 10;
SIGSEGV   := 11;
SIGUSR2   := 12;
SIGPIPE   := 13;
SIGALRM   := 14;
SIGTERM   := 15;
SIGSTKFLT := 16;
SIGCHLD   := 17;
SIGCONT   := 18;
SIGSTOP   := 19;
SIGTSTP   := 20;
SIGTTIN   := 21;
SIGTTOU   := 22;
SIGURG    := 23;
SIGXCPU   := 24;
SIGXFSZ   := 25;
SIGVTALRM := 26;
SIGPROF   := 27;
SIGWINCH  := 28;
SIGIO     := 29;
SIGPOLL   := 29;
SIGPWR    := 30;
SIGSYS    := 31;
SIGUNUSED := SIGSYS;

type Thread: struct {
    stack:ptr;
    start:fn();
    args:[]Any;
};

extern fn __clone(fn(), #autocast ptr, #autocast ptr, #autocast ptr);

STACK_SIZE := 4096;

fn threadCreate(start:fn()) -> &Thread {
    t:Thread;
    // TODO: using MAP_GROWSDOWN causes a segfault ONLY when using `make`
    //t.stack = syscall.mmap(0, STACK_SIZE, PROT_READ | PROT_WRITE,
        //MAP_PRIVATE | MAP_ANON | MAP_GROWSDOWN, -1, 0);
    t.stack = syscall.mmap(0, STACK_SIZE, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    flags := CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|CLONE_PARENT|CLONE_THREAD|CLONE_IO;
    // TODO: is this necessary? or does the presence of certain flags return the
    // top of the stack already from mmap?
    s := t.stack as int + STACK_SIZE;
    __clone(start, s, flags, 0);
    return &t;
}
