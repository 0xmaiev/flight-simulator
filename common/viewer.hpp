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

/*
    Layout of one vertex inside a Model's VBO (interleaved):

        | position (3 floats) | normal (3 floats) | uv (2 floats) |

    The shader's `layout(location = N)` must match these attribute numbers.
*/
#define VERTEX_ATTRIB_POSITION  0
#define VERTEX_ATTRIB_NORMAL    1
#define VERTEX_ATTRIB_UV        2
#define FLOATS_PER_VERTEX       8

// Module defaults
static const GLint DEFAULT_FRAMEBUFFER_WIDTH    = 800;
static const GLint DEFAULT_FRAMEBUFFER_HEIGHT   = 800;
static const GLfloat DEFAULT_AMBIENT_STRENGTH   = 0.15f;

// Exposed defaults
const float KEY_MOVE_SPEED      = 1.5f;     // world units per second
const float KEY_ROTATE_SPEED    = 1.5f;     // radians per second
const float MOUSE_ROTATE_SPEED  = 0.01f;    // radians per pixel
const float MOUSE_MOVE_SPEED    = 0.005f;   // world units per pixel
const float SCROLL_MOVE_SPEED   = 0.2f;     // world units per scroll step

/*
    One placement of a Model in the scene - what the mouse/keyboard move.
    Several instances can share the same Model (e.g. four tables in a room).
*/
struct Instance {
    // index into the models vector; an index (unlike a Model*) stays valid
    // when that vector grows and reallocates
    size_t model;
    glm::vec3 position      = glm::vec3(0.0f);
    // rotation as a quaternion: incremental rotations compose without gimbal lock
    glm::quat orientation   = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity (w, x, y, z)
    float scale             = 1.0f;

    Instance(size_t m): model(m) {}

    /*
        Model matrix M = T * R * S: applied right to left, a vertex is scaled first,
        then rotated around the origin, then translated to `position`.
    */
    glm::mat4 ModelMatrix() const {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 R = glm::mat4_cast(orientation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
        return T * R * S;
    }
};

// Instances of models loaded, each model can have more than one instance. 
typedef std::vector<Instance> Instances;

// Models loaded into GPU and ready to be rendered.
typedef std::vector<Model> Models;

/*
    The contents of a scene, it's object models, textures and instances.
    This is pretty much all that is shared between viewer and simulator.
*/
struct Scene {
    // loaded models, can have more than one instance each
    Models models;
    // object instances vector
    Instances instances;
    // texture cache
    TextureCache textures;
};

/*
    Point light in world space.
*/
struct PointLight {
    glm::vec3 position      = glm::vec3(0.8f, 1.0f, 1.5f);
    glm::vec3 color         = glm::vec3(1.0f, 1.0f, 1.0f);
    float ambient_strength  = DEFAULT_AMBIENT_STRENGTH;
    glm::vec3 attenuation   = glm::vec3(1.0f, 0.05f, 0.01f);
};

/*
    Where we look from, each mode moves in it's own way.
    Viewer has static camera, flight simulator has dynamic camera.
*/
struct Camera {
    glm::vec3 position  = glm::vec3(0.0f, 1.2f, 4.0f);
    glm::vec3 target    = glm::vec3(0.0f, 0.0f, 0.0f);

    glm::mat4 View() const;
};

/*
    Information about the current viewport.
*/
struct Viewport {
    GLint framebuffer_width     = DEFAULT_FRAMEBUFFER_WIDTH;
    GLint framebuffer_height    = DEFAULT_FRAMEBUFFER_HEIGHT;

    glm::mat4 Projection() const;
};

/*
    Run the .OBJ model viewer.
*/
int RunViewer(GLFWwindow *window, const char *path);

#endif
