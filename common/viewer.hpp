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

/*
    Textures already uploaded to the GPU, keyed by their resolved file path,
    so two materials (or two models) using the same image share one texture.
    A value of 0 means loading failed - cached too, so we don't retry every model.
*/
typedef std::map<std::string, GLuint> TextureCache;

/*
    GPU side material, built from a `material_t` with defaults filled in.
*/
struct Material {
    // Phong reflection coefficients
    glm::vec3 ka, kd, ks;
    // Phong specular exponent
    float shininess;
    // diffuse (base color) texture, 0 = none; owned by the TextureCache
    GLuint diffuseTex;

    Material() {
        ka          = glm::vec3(1.0f);      // ambient is scaled by the light's ambient strength
        kd          = glm::vec3(0.8f);      // light gray
        ks          = glm::vec3(0.5f);
        shininess   = 32.0f;
        diffuseTex  = 0;
    };
};

/*
    A range of consecutive vertices in a Model's VBO that share one material,
    drawn with a single glDrawArrays call.
*/
struct Submesh {
    // index of the first vertex (in vertices, not floats)
    GLint first;
    // number of vertices (3 per triangle)
    GLsizei count;
    // index into Model::materials
    int material;

    Submesh() {
        first       = 0;
        count       = 0;
        material    = 0;
    };
};

/*
    Everything needed to draw one loaded .OBJ: built once from an `obj_t`,
    after which the `obj_t` can be discarded.
*/
struct Model {
    GLuint vao, vbo;
    // one per material actually used, in VBO order
    std::vector<Submesh> submeshes;
    // [0] is a default material for faces without `usemtl`,
    // [i + 1] is converted from obj_t::mats[i]
    std::vector<Material> materials;
    // axis-aligned bounding box of the positions
    glm::vec3 bbox_min, bbox_max;
    // centers the model on the origin and scales its longest half-extent to 1,
    // so any downloaded model shows up at a sensible size
    glm::mat4 fit;

    Model() {
        vao         = 0;
        vbo         = 0;
        bbox_min    = glm::vec3(0.0f);
        bbox_max    = glm::vec3(0.0f);
        fit         = glm::mat4(1.0f);
    };
};

/*
    One placement of a Model in the scene - what the mouse/keyboard move.
    Several instances can share the same Model (e.g. four tables in a room).
*/
struct Instance {
    // index into the models vector; an index (unlike a Model*) stays valid
    // when that vector grows and reallocates
    size_t model;
    glm::vec3 position;
    // rotation as a quaternion: incremental rotations compose without gimbal lock
    glm::quat orientation;
    float scale;

    Instance(size_t m) {
        model       = m;
        position    = glm::vec3(0.0f);
        orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);   // identity (w, x, y, z)
        scale       = 1.0f;
    };

    /*
        Model matrix M = T * R * S: scale first, then rotate, then translate.
    */
    glm::mat4 ModelMatrix() const;
};

/*
    Attempts loading an image file into a GL texture, using the cache.
    Returns 0 if it can't be loaded.
*/
GLuint LoadTexture(const std::string &path, TextureCache *cache);

/*
    Deletes every texture in the cache and empties it.
*/
void DeleteTextures(TextureCache *cache);

/*
    Attempts building the required OpenGL buffers to render the given .OBJ.
*/
Model BuildModel(const obj_t &object, TextureCache *textures);

/*
    Draws every submesh of the model with the given (already bound) program.
    The program's model/view/projection/light uniforms must be set by the caller;
    this only sets the per-material ones.
*/
void DrawModel(const Model &model, GLuint program);

/*
    Deletes the model's VAO/VBO (textures belong to the TextureCache).
*/
void DeleteModel(Model *model);

/*
    View the object at the given path
*/
int ViewModel(GLFWwindow *window, const char *path);

#endif
