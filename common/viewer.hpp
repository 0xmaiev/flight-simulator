/*
    An attempt at a very tiny OpenGL .OBJ Viewer.

    Source material used as reference:
    https://www.di.ubi.pt/~agomes/cg/assignments/02-assignment.pdf
    https://www.fileformat.info/format/material/

    Author: Hugo Carvalho <a36295@ubi.pt>
    Email: hugo.carvalho@ubi.pt
*/

#ifndef VIEWER_HPP
#define VIEWER_HPP

// Include standard headers
#include <map>
#include <string>
#include <vector>

// Include OpenGL related headers
#include <glm/glm.hpp>                  // GLM header file
#include <glm/gtc/quaternion.hpp>       // GLM quaternions
#include <GL/glew.h>                    // GLEW header file
#include <GLFW/glfw3.h>                 // GLFW header file

// Include our own header files
#include <common/model.hpp>
#include <common/objloader.hpp>


// Exposed defaults
const float KEY_MOVE_SPEED      = 1.5f;     // world units per second
const float KEY_ROTATE_SPEED    = 1.5f;     // radians per second
const float MOUSE_ROTATE_SPEED  = 0.01f;    // radians per pixel
const float MOUSE_MOVE_SPEED    = 0.005f;   // world units per pixel
const float SCROLL_MOVE_SPEED   = 0.2f;     // world units per scroll step

/*
    Run the .OBJ model viewer.
*/
int RunViewer(GLFWwindow *window, const char *path);

#endif
