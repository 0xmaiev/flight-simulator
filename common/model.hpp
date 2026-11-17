
#ifndef MODEL_HPP
#define MODEL_HPP

#include <vector>

#include <glm/glm.hpp>                  // GLM header file
#include <GLFW/glfw3.h>                 // GLFW header file

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
    glm::vec3 ka        = glm::vec3(1.0f);
    glm::vec3 kd        = glm::vec3(0.8f);
    glm::vec3 ks        = glm::vec3(0.5f);
    // Phong specular exponent
    float shininess     = 32.0f;
    // diffuse (base color) texture, 0 = none; owned by the TextureCache
    GLuint diffuseTex   = 0;
};

/*
    A range of consecutive vertices in a Model's VBO that share one material,
    drawn with a single glDrawArrays call.
*/
struct Submesh {
    // index of the first vertex (in vertices, not floats)
    GLint first     = 0;
    // number of vertices (3 per triangle)
    GLsizei count   = 0;
    // index into Model::materials
    int material    = 0;
};

/*
    Everything needed to draw one loaded .OBJ: built once from an `obj_t`,
    after which the `obj_t` can be discarded.
*/
struct Model {
    GLuint vao                          = 0;
    GLuint vbo                          = 0;
    // one per material actually used, in VBO order
    std::vector<Submesh> submeshes;
    // [0] is a default material for faces without `usemtl`,
    // [i + 1] is converted from obj_t::mats[i]
    std::vector<Material> materials;
    // axis-aligned bounding box of the positions
    glm::vec3 bbox_min                  = glm::vec3(0.0f);
    glm::vec3 bbox_max                  = glm::vec3(0.0f);
    // centers the model on the origin and scales its longest half-extent to 1,
    // so any downloaded model shows up at a sensible size
    glm::mat4 fit                       = glm::mat4(1.0f);
};

// Models loaded into GPU and ready to be rendered.
typedef std::vector<Model> Models;

/*
    Attempts loading an image file into a GL texture, using the cache.
*/
GLuint LoadTexture(const string &path, TextureCache *cache);

/*
    Attempts to load a cube map stored as a 4x3 horizontal cross:
            [+Y]
      [-X]  [+Z]  [+X]  [-Z]
            [-Y]
*/
GLuint LoadCubemapCross(const std::string &path, TextureCache *cache);

/*
    Deletes every texture in the cache and empties it.
*/
void DeleteTextures(TextureCache *cache);

/*
    Attempts building the required OpenGL buffers to render the given .OBJ.

    The .OBJ indexes positions, normals and UVs separately (`f v/vt/vn`), but OpenGL
    has a single index per vertex. So we "de-index": every triangle corner gets its
    own copy of position + normal + uv in one interleaved buffer, drawn with glDrawArrays.

    Triangles are grouped by material (not by shape) so each material is one draw call.
*/
Model BuildModel(const obj_t &obj, TextureCache *textures);

/*
    Draws every submesh of the model with the given (already bound) program.
*/
void DrawModel(const Model &model, GLuint program);

/*
    Deletes the model's VAO/VBO (textures belong to the TextureCache).
*/
void DeleteModel(Model *model);

#endif