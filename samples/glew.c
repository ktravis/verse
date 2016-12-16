#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

void glewExperimentalOn() {
    glewExperimental = GL_TRUE;
}
