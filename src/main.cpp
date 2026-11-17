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
#include <GL/glew.h>                    // GLEW header file
#include <GLFW/glfw3.h>                 // GLFW header file

// Our own header files
#include <common/viewer.hpp>            // very tiny OBJ viewer header file
#include <common/simulator.hpp>         // very tiny simulator header file

// Global constants
const GLint WINDOW_WIDTH = 800;
const GLint WINDOW_HEIGHT = 800;

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
    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
        windowName, NULL, NULL);

    if (!window) {
        fprintf(stderr, "failed to create opengl window\n");
        glfwTerminate();
        return 1;
    }

    // Create window context
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Needed for core profile and other modern features
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "failed to initialize glew\n");
        return 1;
    };
    

    // if we got arguments check which
    if (argc > 1) {
        // utility to help debug/load meshes
        // ./program /path/to/.obj
        return RunViewer(window, argv[1]);
    } else {
        // run simulator by default
        RunSimulator(window);
    }

    return 0;
}
