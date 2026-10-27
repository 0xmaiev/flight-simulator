/*
    It runs a minimal flight simulator.

    Controls: (TODO)

    Author: Hugo Carvalho <a36295@ubi.pt>
    Email: hugo.carvalho@ubi.pt
*/


// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <cmath>

using namespace std;

// Include OpenGL related headers
#include <glm/glm.hpp>                  // GLM header file
#include <glm/gtc/matrix_transform.hpp> // GLM matrix transform ops
#include <glm/gtc/matrix_inverse.hpp>   // GLM inverseTranspose
#include <glm/gtc/type_ptr.hpp>         // GLM type pointers
#include <GL/glew.h>                    // GLEW header file
#include <GLFW/glfw3.h>                 // GLFW header file

using namespace glm;

// Our own header files
#include <common/objloader.hpp>         // very tiny OBJ loader header file
#include <common/viewer.hpp>            // very tiny OBJ viewer header file
#include <common/simulator.hpp>         // very tiny simulator header file
#include <common/shader.hpp>            // GLSL shader header file

GLint windowWidth = 800;
GLint windowHeight = 800;
GLFWwindow* window;             // graphics window of this program

//--------------------------------------------------------------------------------

int main(int argc, char *argv[] )
{
    // Initialise GLFW
    if (!glfwInit()) {
        fprintf(stderr, "failed to initialize glfw\n");
        return 1;
    };

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const char *windowName = argc > 1 ? "Mesh Renderer" : "Flight Simulator";
    // Open a window
    window = glfwCreateWindow(windowWidth, windowHeight, windowName, NULL, NULL);

    // if we got arguments check which
    if (argc > 1) {
        // utility to help debug/load meshes
        // ./program view /path/to/.obj
        return ViewModel(window, argv[1]);
    } else {
        // run simulator by default
        RunSimulator(window);
    }

    if (!window) {
        fprintf(stderr, "failed to create opengl window\n");
        glfwTerminate();
        return 1;
    }

    return 0;
}
