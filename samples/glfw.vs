#import "samples/glfw_defs.vs"

extern fn glfwInit();
extern fn glfwTerminate();
extern fn glfwWindowHint(GLFWOption, GLFWOption);
extern fn glfwCreateWindow(int, int, &u8, &GLFWmonitor, &GLFWwindow):&GLFWwindow;
extern fn glfwMakeContextCurrent(&GLFWwindow);
extern fn glfwGetFramebufferSize(&GLFWwindow, &int, &int);
extern fn glfwWindowShouldClose(&GLFWwindow):bool;
extern fn glfwPollEvents();
extern fn glfwSwapBuffers(&GLFWwindow);
extern fn glfwSetKeyCallback(&GLFWwindow, fn(&GLFWwindow, KeyCode, int, InputAction, InputMods));
extern fn glfwSetWindowShouldClose(&GLFWwindow, bool);

type GLFWwindow : struct {
    _ : bool;
};
type GLFWmonitor : struct {
    _ : bool;
};

emptyWindow:&GLFWwindow;
defaultMonitor:&GLFWmonitor;

type Color : struct {
    r:float32;
    g:float32;
    b:float32;
    a:float32;
};

extern fn glewInit():int;
extern fn glViewport(int, int, int, int);
extern fn glewExperimentalOn();
extern fn glClearColor(float,float,float,float);
extern fn glClear(int);

fn key_callback(window:&GLFWwindow, key:KeyCode, scancode:int, action:InputAction, mode:InputMods) {
    use KeyCode;
    use InputAction;

    if key == KEY_ESCAPE && action == PRESS {
        glfwSetWindowShouldClose(window, true);
    }
}

fn main():int {
    use GLFWOption;

    glfwInit();
    glfwWindowHint(CONTEXT_VERSION_MAJOR, THREE);
    glfwWindowHint(CONTEXT_VERSION_MINOR, THREE);
    glfwWindowHint(OPENGL_PROFILE, OPENGL_CORE_PROFILE);
    glfwWindowHint(RESIZABLE, FALSE);

    window := glfwCreateWindow(800, 600, "Learn OpenGL".bytes, defaultMonitor, emptyWindow);
    if !validptr(window as ptr) {
        assert(false);
    }

    glfwMakeContextCurrent(window);
    glewExperimentalOn();

    if (glewInit() != 0) {
        println("done goofed");
        return -1;
    }

    width:int;
    height:int;

    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glfwSetKeyCallback(window, key_callback);

    while !glfwWindowShouldClose(window) {
        glfwPollEvents();

        glClearColor(0.2, 0.3, 0.3, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        glfwSwapBuffers(window);
    }
    glfwTerminate();

    println("All good yo");

    return 0;
}
