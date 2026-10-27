/*
    An attempt at a very tiny OpenGL .OBJ Viewer.

    Source material used as reference:
    https://www.di.ubi.pt/~agomes/cg/assignments/02-assignment.pdf
    https://www.fileformat.info/format/wavefrontobj/egff.htm
	https://www.fileformat.info/format/material/
    https://github.com/tinyobjloader/tinyobjloader/tree/release/examples/viewer

    Author: Hugo Carvalho <a36295@ubi.pt>
    Email: hugo.carvalho@ubi.pt
*/

// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include <string> 
#include <vector>

using namespace std;

// Include OpenGL related headers
#include <glm/gtc/matrix_transform.hpp> // GLM matrix transform ops
#include <glm/gtc/matrix_inverse.hpp>   // GLM inverseTranspose
#include <glm/gtc/type_ptr.hpp>         // GLM type pointers
#include <GL/glew.h>                    // GLEW header file
#include <GLFW/glfw3.h>                 // GLFW header file

using namespace glm;

// Our own header files
#include <common/objloader.hpp>         // very tiny OBJ loader header file
#include <common/viewer.hpp>            // very tiny OBJ viewer header file
#include <common/stb_image.h>           // image loading (implementation in stb_image.cpp)
#include <common/shader.hpp>            // GLSL shader header file

//GLuint phongProgramID;          // GLSL program for the meshes (Phong shading)
//GLuint linesProgramID;          // GLSL program for the axes and light marker (unlit)

GLuint axesVertexArrayID;       // Vertex array object for the X/Y/Z axes
GLuint axesVertexBuffer;        // Vertex buffer object for the X/Y/Z axes
GLuint axesColorBuffer;         // color buffer object (CBO)

GLuint lightVertexArrayID;      // Vertex array object for the light marker
GLuint lightVertexBuffer;       // Vertex buffer object for the light marker
GLuint lightColorBuffer;        // color buffer object (CBO)

GLint framebufferWidth = 800;   // in pixels; differs from the window size on HiDPI screens
GLint framebufferHeight = 800;

// Scene: models are the GPU data, instances are placements of them that the user moves
vector<Model> models;
vector<Instance> instances;
TextureCache textures;
size_t selected = 0;            // the instance that mouse/keyboard input moves

// Camera: fixed, slightly above the scene, looking at the origin
const vec3 cameraPosition(0.0f, 1.2f, 4.0f);
const vec3 cameraTarget(0.0f, 0.0f, 0.0f);

// Point light, in world space
const vec3 lightPosition(0.8f, 1.0f, 1.5f);
const vec3 lightColor(1.0f, 1.0f, 1.0f);
const float ambientStrength = 0.15f;
const vec3 lightAttenuation(1.0f, 0.05f, 0.01f);   // constant, linear, quadratic

// Input
const float keyMoveSpeed = 1.5f;        // world units per second
const float keyRotateSpeed = 1.5f;      // radians per second
const float mouseRotateSpeed = 0.01f;   // radians per pixel
const float mouseMoveSpeed = 0.005f;    // world units per pixel
const float scrollMoveSpeed = 0.2f;     // world units per scroll step

bool rotating = false;          // left mouse button held
bool panning = false;           // right mouse button held
double lastCursorX = 0.0;
double lastCursorY = 0.0;
bool wireframe = false;

// --------------------------------------------------------------------------------
/*
    Listing function prototypes
*/
// --------------------------------------------------------------------------------

// callbacks

// todo: move framebufferSizeCallback to reusable namespace
// keep all others in this source file because they are specific viewer controls
void framebufferSizeCallback(GLFWwindow* window, int width, int height);

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

static void processKeyboard(GLFWwindow *window, float deltaTime);

// todo: move these to reusable namespace as well
void rotateSelectedAroundWorldAxis(float angle, const vec3 &axis);
void rotateSelectedAroundLocalAxis(float angle, const vec3 &axis);

bool transferDataToGPUMemory(const vector<obj_t> &objects, std::string &error);
void cleanupDataFromGPU(void);

static int draw(std::string& error);

/*
    Model matrix M = T * R * S: applied right to left, a vertex is scaled first,
    then rotated around the origin, then translated to `position`.
*/
glm::mat4 Instance::ModelMatrix() const {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 R = glm::mat4_cast(orientation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
    return T * R * S;
}

/*
    Joins an .MTL texture path onto the .OBJ's directory.
*/
static string resolvePath(const string &base_dir, string relpath) {
    // files exported on Windows sometimes use backslashes
    replace(relpath.begin(), relpath.end(), '\\', '/');
    if (!relpath.empty() && relpath[0] == '/') {
        return relpath;     // already absolute
    }
    return base_dir + "/" + relpath;
}

/*
    Attempts loading an image file into a GL texture, using the cache.
*/
GLuint LoadTexture(const string &path, TextureCache *cache) {
    // already loaded (or already failed) - share it
    TextureCache::iterator it = cache->find(path);
    if (it != cache->end()) {
        return it->second;
    }

    fprintf(stdout, "loading texture: %s\n", path.c_str());

    // image files store rows top to bottom, but UV (0,0) is the *bottom*-left
    // corner in both .OBJ and OpenGL: flip on load so UVs can be used unchanged
    stbi_set_flip_vertically_on_load(1);

    // ask for 4 channels so grayscale, RGB and RGBA images all upload the same way;
    // 16-bit images (like the table's) are converted down to 8 bits per channel
    int width, height, channels;
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) {
        fprintf(stderr, "failed to load texture %s: %s\n", path.c_str(), stbi_failure_reason());
        (*cache)[path] = 0;
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    // mipmaps: pre-shrunk copies so the texture doesn't shimmer when the model is far away
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(data);

    fprintf(stdout, "loaded texture: %dx%d, %d channels\n", width, height, channels);
    (*cache)[path] = texture;
    return texture;
}

/*
    Deletes every texture in the cache and empties it.
*/
void DeleteTextures(TextureCache *cache) {
    for (TextureCache::iterator it = cache->begin(); it != cache->end(); ++it) {
        if (it->second != 0) {
            glDeleteTextures(1, &it->second);
        }
    }
    cache->clear();
}

/*
    Converts a parsed `material_t` into a GPU side `Material`, filling in defaults.
*/
static Material convertMaterial(const material_t &m, const string &base_dir, TextureCache *textures) {
    Material out;

    out.ka = glm::vec3(m.ambient[0], m.ambient[1], m.ambient[2]);
    out.kd = glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]);
    out.ks = glm::vec3(m.specular[0], m.specular[1], m.specular[2]);

    if (m.shininess > 0.0f) {
        out.shininess = m.shininess;
    } else {
        out.shininess = 32.0f;      // `Ns` not given (Table.mtl has none)
    }

    if (!m.diffuse_relpath.empty()) {
        out.diffuseTex = LoadTexture(resolvePath(base_dir, m.diffuse_relpath), textures);
    }

    // A missing statement parses as 0, which can't be told apart from an explicit 0.
    // For Kd and Ka an all-zero value would render the model black, so treat it as
    // "not given":
    //  - Kd: the texture color is multiplied by Kd, so default to 1 with a texture
    //        (Table.mtl has map_Kd but no Kd), light gray without one
    //  - Ka: many exporters write `Ka 0 0 0` meaning "use the scene's ambient light"
    if (out.kd == glm::vec3(0.0f)) {
        out.kd = out.diffuseTex ? glm::vec3(1.0f) : glm::vec3(0.8f);
    }
    if (out.ka == glm::vec3(0.0f)) {
        out.ka = glm::vec3(1.0f);
    }

    return out;
}

/*
    Appends one vertex (position, normal, uv) to an interleaved buffer.
*/
static void pushVertex(vector<float> &out, const glm::vec3 &p, const glm::vec3 &n, const glm::vec2 &uv) {
    out.push_back(p.x);
    out.push_back(p.y);
    out.push_back(p.z);
    out.push_back(n.x);
    out.push_back(n.y);
    out.push_back(n.z);
    out.push_back(uv.x);
    out.push_back(uv.y);
}

/*
    Attempts building the required OpenGL buffers to render the given .OBJ.

    The .OBJ indexes positions, normals and UVs separately (`f v/vt/vn`), but OpenGL
    has a single index per vertex. So we "de-index": every triangle corner gets its
    own copy of position + normal + uv in one interleaved buffer, drawn with glDrawArrays.

    Triangles are grouped by material (not by shape) so each material is one draw call.
*/
Model BuildModel(const obj_t &obj, TextureCache *textures) {
    Model model;

    const attribute_t &attrib = obj.attr;
    const vector<shape_t> &shapes = obj.shapes;
    const vector<material_t> &materials = obj.mats;

    // materials: slot 0 is the default for faces without (or with an unknown) `usemtl`,
    // slot i + 1 is materials[i]
    model.materials.push_back(Material());
    for (size_t i = 0; i < materials.size(); i++) {
        model.materials.push_back(convertMaterial(materials[i], obj.base_dir, textures));
    }

    // one bucket of interleaved vertices per material slot
    vector< vector<float> > buckets(model.materials.size());

    size_t skipped = 0;
    bool first_vertex = true;

    // traverse the object's shapes and de-index, grouping triangles by material
    for (size_t s = 0; s < shapes.size(); s++) {
        const mesh_t &mesh = shapes[s].mesh;

        // the loader already triangulated everything: 3 indices per face
        for (size_t f = 0; f < mesh.indices.size() / 3; f++) {
            const index_t *idx = &mesh.indices[3 * f];

            // skip triangles that reference vertices that don't exist (broken files)
            bool valid = true;
            for (int k = 0; k < 3; k++) {
                if (idx[k].vertex_idx < 0 || idx[k].vertex_idx >= (int)attrib.vertices.size()) {
                    valid = false;
                }
            }
            if (!valid) {
                skipped++;
                continue;
            }

            // one material id per triangle, -1 (no material) goes to slot 0
            int material_id = f < mesh.material_ids.size() ? mesh.material_ids[f] : -1;
            int slot = (material_id >= 0 && material_id < (int)materials.size()) ? material_id + 1 : 0;
            vector<float> &out = buckets[slot];

            glm::vec3 v[3];
            for (int k = 0; k < 3; k++) {
                v[k] = attrib.vertices[idx[k].vertex_idx];
            }

            // flat face normal, used for corners that have no `vn`
            glm::vec3 face_normal = glm::cross(v[1] - v[0], v[2] - v[0]);
            float len = glm::length(face_normal);
            face_normal = len > 0.0f ? face_normal / len : glm::vec3(0.0f, 1.0f, 0.0f);

            for (int k = 0; k < 3; k++) {
                glm::vec3 n = face_normal;
                if (idx[k].normal_idx >= 0 && idx[k].normal_idx < (int)attrib.normals.size()) {
                    n = attrib.normals[idx[k].normal_idx];
                }

                glm::vec2 uv(0.0f, 0.0f);
                if (idx[k].text_coord_idx >= 0 && idx[k].text_coord_idx < (int)attrib.text_coords.size()) {
                    uv = attrib.text_coords[idx[k].text_coord_idx];
                }

                pushVertex(out, v[k], n, uv);

                // grow the bounding box
                if (first_vertex) {
                    model.bbox_min = model.bbox_max = v[k];
                    first_vertex = false;
                } else {
                    model.bbox_min = glm::min(model.bbox_min, v[k]);
                    model.bbox_max = glm::max(model.bbox_max, v[k]);
                }
            }
        }
    }

    // concatenate the buckets into one buffer; each non-empty bucket becomes a submesh
    vector<float> buffer;
    for (size_t b = 0; b < buckets.size(); b++) {
        if (buckets[b].empty()) {
            continue;
        }
        Submesh submesh;
        submesh.first    = (GLint)(buffer.size() / FLOATS_PER_VERTEX);     // in vertices, not floats
        submesh.count    = (GLsizei)(buckets[b].size() / FLOATS_PER_VERTEX);
        submesh.material = (int)b;
        buffer.insert(buffer.end(), buckets[b].begin(), buckets[b].end());
        model.submeshes.push_back(submesh);
    }

    if (skipped > 0) {
        fprintf(stderr, "skipped %zu triangles with invalid vertex indices\n", skipped);
    }
    if (buffer.empty()) {
        fprintf(stderr, "model has no triangles to draw\n");
        return model;
    }

    // fit: move the bounding box center to the origin, then scale the longest half-extent to 1
    glm::vec3 center = (model.bbox_min + model.bbox_max) * 0.5f;
    glm::vec3 half = (model.bbox_max - model.bbox_min) * 0.5f;
    float radius = glm::max(half.x, glm::max(half.y, half.z));
    float s = radius > 0.0f ? 1.0f / radius : 1.0f;
    model.fit = glm::scale(glm::mat4(1.0f), glm::vec3(s)) * glm::translate(glm::mat4(1.0f), -center);

    // init vao: it records the buffer binding + attribute layout below
    glGenVertexArrays(1, &model.vao);
    glBindVertexArray(model.vao);

    // init vbo and upload the vertices
    glGenBuffers(1, &model.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, model.vbo);
    glBufferData(GL_ARRAY_BUFFER, buffer.size() * sizeof(float), buffer.data(), GL_STATIC_DRAW);

    // describe the interleaved layout: stride = size of one whole vertex,
    // offset = where the attribute starts inside a vertex
    const GLsizei stride = FLOATS_PER_VERTEX * sizeof(float);
    glVertexAttribPointer(VERTEX_ATTRIB_POSITION, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glVertexAttribPointer(VERTEX_ATTRIB_NORMAL,   3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glVertexAttribPointer(VERTEX_ATTRIB_UV,       2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION);
    glEnableVertexAttribArray(VERTEX_ATTRIB_NORMAL);
    glEnableVertexAttribArray(VERTEX_ATTRIB_UV);

    // unbind the VAO first, then the VBO, so the VAO keeps its state
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    fprintf(stdout, "built model: %zu triangles, %zu submeshes\n",
            buffer.size() / FLOATS_PER_VERTEX / 3, model.submeshes.size());
    for (size_t i = 0; i < model.submeshes.size(); i++) {
        const Submesh &sm = model.submeshes[i];
        fprintf(stdout, "\tsubmesh[%zu]: first=%d count=%d material=%d\n", i, sm.first, sm.count, sm.material);
    }

    return model;
}

/*
    Draws every submesh of the model with the given (already bound) program.
*/
void DrawModel(const Model &model, GLuint program) {
    if (model.vao == 0) {
        return;
    }

    // looked up per call for simplicity; with many models, cache these once
    GLint ka_loc          = glGetUniformLocation(program, "materialKa");
    GLint kd_loc          = glGetUniformLocation(program, "materialKd");
    GLint ks_loc          = glGetUniformLocation(program, "materialKs");
    GLint shininess_loc   = glGetUniformLocation(program, "materialShininess");
    GLint has_tex_loc     = glGetUniformLocation(program, "hasDiffuseTexture");
    GLint tex_loc         = glGetUniformLocation(program, "diffuseTexture");

    // the sampler reads from texture unit 0
    glUniform1i(tex_loc, 0);
    glActiveTexture(GL_TEXTURE0);

    glBindVertexArray(model.vao);
    for (size_t i = 0; i < model.submeshes.size(); i++) {
        const Submesh &submesh = model.submeshes[i];
        const Material &material = model.materials[submesh.material];

        glUniform3fv(ka_loc, 1, glm::value_ptr(material.ka));
        glUniform3fv(kd_loc, 1, glm::value_ptr(material.kd));
        glUniform3fv(ks_loc, 1, glm::value_ptr(material.ks));
        glUniform1f(shininess_loc, material.shininess);
        glUniform1i(has_tex_loc, material.diffuseTex != 0);
        glBindTexture(GL_TEXTURE_2D, material.diffuseTex);

        glDrawArrays(GL_TRIANGLES, submesh.first, submesh.count);
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/*
    Deletes the model's VAO/VBO (textures belong to the TextureCache).
*/
void DeleteModel(Model *model) {
    glDeleteBuffers(1, &model->vbo);
    glDeleteVertexArrays(1, &model->vao);
    model->vbo = 0;
    model->vao = 0;
}


/*
    View the object at the given path
*/
int ViewModel(GLFWwindow *window, const char *path) {
    // Create window context
    glfwMakeContextCurrent(window);

    // set callbacks: resizing, and the GLFW mouse/keyboard input
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);

    // the framebuffer can be bigger than the window (HiDPI), so ask for its real size
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    // Initialize GLEW
    glewExperimental = GL_TRUE; // Needed for core profile and other modern features
    GLenum err = glewInit();
    if (GLEW_OK != err) {
        fprintf(stderr, "failed to initialize glew\n");
        return 1;
    };

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    // enable depth testing
    glEnable(GL_DEPTH_TEST);

    // Blue background
    glClearColor(0.0f, 0.0f, 0.15f, 0.0f);

    // if we have an obj definition file with multiple objects we
    // iterate over all of them here and load them onto a vector<>
    // leaving rest as is because it's not scope for this assignment
    // just preparing for the future with some context!
    vector<obj_t> objects;
    obj_t obj;

    // load table object
    if ( !LoadObj(path, &obj) ) {
        fprintf(stderr, "failed to load obj\n");
        return 1;
    }
    // push onto the vector
    objects.push_back(obj);
    
    std::string error;

    // transfer my data (shaders, axes, models, textures) to GPU side
    if (!transferDataToGPUMemory(objects, error)) {
        glfwTerminate();
        if (!error.empty()) {
            fprintf(stderr, "%s\n", error.c_str());
        }
        return 1;
    }

    // the GPU has its own copy now: the parsed .OBJ data can be freed
    objects.clear();

    fprintf(stdout,
        "\ncontrols:\n"
        "  left mouse drag   rotate\n"
        "  right mouse drag  translate X/Y\n"
        "  mouse scroll      translate Z\n"
        "  arrow keys        translate X/Y\n"
        "  Page Up / Down    translate Z\n"
        "  A/D W/S Q/E       rotate around Y / X / Z\n"
        "  R                 reset\n"
        "  F                 toggle wireframe\n"
        "  Esc               quit\n");

    double lastTime = glfwGetTime();
    int drawResult;

    // render scene for each frame
    do {
        // time since the last frame, so key movement speed doesn't depend on the frame rate
        double now = glfwGetTime();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;

        // looking for events (calls the callbacks)
        glfwPollEvents();
        // keys held down
        processKeyboard(window, deltaTime);
        // drawing callback
        drawResult = draw(error);
        if (drawResult == -1) {
            break;
        }
        // Swap buffers
        glfwSwapBuffers(window);

    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
          glfwWindowShouldClose(window) == GL_FALSE );

    // Cleanup VAO, VBOs, and shaders from GPU
    cleanupDataFromGPU();

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    if (drawResult == -1 ){
        if (!error.empty()) {
            fprintf(stderr, "%s\n", error.c_str());
        }
        return 2;
    }

    return 0;
}


// --------------------------------------------------------------------------------
/*
    Callbacks
*/

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
    // remembered so the projection's aspect ratio follows the window (see draw)
    framebufferWidth = width;
    framebufferHeight = height;
}

/*
    One-shot keys: called once per press. Keys that are held down
    are polled every frame in processKeyboard instead.
*/
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS || instances.empty()) {
        return;
    }

    switch (key) {
        case GLFW_KEY_R: {
            // reset the selected instance's placement
            Instance &instance = instances[selected];
            instance.position = vec3(0.0f);
            instance.orientation = quat(1.0f, 0.0f, 0.0f, 0.0f);
            break;
        }
        case GLFW_KEY_F:
            wireframe = !wireframe;
            break;
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        rotating = (action == GLFW_PRESS);
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        panning = (action == GLFW_PRESS);
    }
    // start measuring the drag from where the button was pressed
    glfwGetCursorPos(window, &lastCursorX, &lastCursorY);
}

void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    // cursor movement since the last event, in pixels (y grows downwards)
    float dx = (float)(xpos - lastCursorX);
    float dy = (float)(ypos - lastCursorY);
    lastCursorX = xpos;
    lastCursorY = ypos;

    if (instances.empty()) {
        return;
    }

    if (rotating) {
        // horizontal drag spins around the vertical axis, vertical drag tips it forwards/backwards
        rotateSelectedAroundWorldAxis(dx * mouseRotateSpeed, vec3(0.0f, 1.0f, 0.0f));
        rotateSelectedAroundWorldAxis(dy * mouseRotateSpeed, vec3(1.0f, 0.0f, 0.0f));
    }
    if (panning) {
        instances[selected].position.x += dx * mouseMoveSpeed;
        instances[selected].position.y -= dy * mouseMoveSpeed;
    }
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    if (instances.empty()) {
        return;
    }
    // scrolling up brings the object closer (+Z points towards the camera)
    instances[selected].position.z += (float)yoffset * scrollMoveSpeed;
}

// --------------------------------------------------------------------------------
/*
    Rotates the selected instance around a *world* axis.

    Multiplying the new rotation on the left applies it after the existing one,
    i.e. around the fixed world axes, so dragging right always spins the object
    the same way on screen no matter how it's already rotated.
*/
void rotateSelectedAroundWorldAxis(float angle, const vec3 &axis) {
    Instance &instance = instances[selected];
    instance.orientation = normalize(angleAxis(angle, axis) * instance.orientation);
}
/*
    Rotates the selected instance around a *local* axis.

    Multiplying the new rotation on the right applies it before the existing one,
    i.e. around the object's own, already rotated axes. This allows mimicking real flight 
    dynamics where pulling back on the stick pitches the nose "up" relative to the 
    pilot, regardless of whether the aircraft is inverted or banked.
*/
void rotateSelectedAroundLocalAxis(float angle, const vec3 &axis) {
    Instance &instance = instances[selected];
    instance.orientation = normalize(instance.orientation * angleAxis(angle, axis));
}

/*
    Keys that act while held down, polled once per frame.
*/
static void processKeyboard(GLFWwindow *window, float deltaTime) {
    if (instances.empty()) {
        return;
    }

    Instance &instance = instances[selected];
    float move = keyMoveSpeed * deltaTime;
    float turn = keyRotateSpeed * deltaTime;

    // translate
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)      instance.position.x -= move;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)     instance.position.x += move;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)      instance.position.y -= move;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)        instance.position.y += move;
    if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) instance.position.z -= move;
    if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)   instance.position.z += move;

    // rotate
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) rotateSelectedAroundWorldAxis(-turn, vec3(0.0f, 1.0f, 0.0f));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) rotateSelectedAroundWorldAxis( turn, vec3(0.0f, 1.0f, 0.0f));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) rotateSelectedAroundWorldAxis(-turn, vec3(1.0f, 0.0f, 0.0f));
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) rotateSelectedAroundWorldAxis( turn, vec3(1.0f, 0.0f, 0.0f));
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) rotateSelectedAroundWorldAxis( turn, vec3(0.0f, 0.0f, 1.0f));
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) rotateSelectedAroundWorldAxis(-turn, vec3(0.0f, 0.0f, 1.0f));
}

// --------------------------------------------------------------------------------
/*
    Creates a VAO for unlit colored lines (position + color, two separate VBOs).
*/
static void createLines(const GLfloat *positions, const GLfloat *colors, GLsizeiptr size,
                        GLuint *vao, GLuint *vbo, GLuint *cbo) {
    glGenVertexArrays(1, vao);
    glBindVertexArray(*vao);

    // 1st attribute buffer : vertices
    glGenBuffers(1, vbo);
    glBindBuffer(GL_ARRAY_BUFFER, *vbo);
    glBufferData(GL_ARRAY_BUFFER, size, positions, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);

    // 2nd attribute buffer : colors
    glGenBuffers(1, cbo);
    glBindBuffer(GL_ARRAY_BUFFER, *cbo);
    glBufferData(GL_ARRAY_BUFFER, size, colors, GL_STATIC_DRAW);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(1);

    // unbind the VAO and VBO safely
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

/*
    Create shaders, VAOs and VBOs and transfer them to graphics card memory
*/
bool transferDataToGPUMemory(const vector<obj_t> &objects, std::string &error)
{
    // Create and compile our GLSL programs from the shaders
    bool ret = LoadShaders("shaders", error);
    if (!ret) {
        return false;
    }

    // Axes, in world space: they stay put while the object moves
    static const GLfloat g_axes_buffer_data[] = {
        -1.5f,  0.0f,  0.0f,   1.5f, 0.0f, 0.0f,    // X-axis
         0.0f, -1.5f,  0.0f,   0.0f, 1.5f, 0.0f,    // Y-axis
         0.0f,  0.0f, -1.5f,   0.0f, 0.0f, 1.5f,    // Z-axis
    };
    static const GLfloat g_axes_color_data[] = {
        1.0f, 0.0f, 0.0f,   1.0f, 0.0f, 0.0f,       // X-axis (Red)
        0.0f, 1.0f, 0.0f,   0.0f, 1.0f, 0.0f,       // Y-axis (Green)
        0.0f, 0.0f, 1.0f,   0.0f, 0.0f, 1.0f,       // Z-axis (Blue)
    };
    createLines(g_axes_buffer_data, g_axes_color_data, sizeof(g_axes_buffer_data),
                &axesVertexArrayID, &axesVertexBuffer, &axesColorBuffer);

    // Light marker: a small yellow 3D cross, moved to the light's position when drawn
    static const GLfloat g_light_buffer_data[] = {
        -1.0f,  0.0f,  0.0f,   1.0f, 0.0f, 0.0f,
         0.0f, -1.0f,  0.0f,   0.0f, 1.0f, 0.0f,
         0.0f,  0.0f, -1.0f,   0.0f, 0.0f, 1.0f,
    };
    static const GLfloat g_light_color_data[] = {
        1.0f, 1.0f, 0.0f,   1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,   1.0f, 1.0f, 0.0f,
    };
    createLines(g_light_buffer_data, g_light_color_data, sizeof(g_light_buffer_data),
                &lightVertexArrayID, &lightVertexBuffer, &lightColorBuffer);

    // then we convert from `obj_t` to `Model`s (shared textures go through the cache)
    for (size_t i = 0; i < objects.size(); i++) {
        models.push_back(BuildModel(objects[i], &textures));
    }

    // and create one `Instance` per model so we can manipulate it; created only
    // after all models exist, and they refer to models by index, not pointer
    for (size_t i = 0; i < models.size(); i++) {
        instances.push_back(Instance(i));
    }

    return true;
}

// --------------------------------------------------------------------------------
/*
    Remove VBOs, VAOs, textures and shaders from graphics card memory
*/
void cleanupDataFromGPU(void) {
    for (size_t i = 0; i < models.size(); i++) {
        DeleteModel(&models[i]);
    }
    DeleteTextures(&textures);

    glDeleteBuffers(1, &axesVertexBuffer);
    glDeleteBuffers(1, &axesColorBuffer);
    glDeleteVertexArrays(1, &axesVertexArrayID);
    glDeleteBuffers(1, &lightVertexBuffer);
    glDeleteBuffers(1, &lightColorBuffer);
    glDeleteVertexArrays(1, &lightVertexArrayID);

    const GLuint phongProgramID = GetShader("phong");
    const GLuint linesProgramID = GetShader("lines");

    glDeleteProgram(phongProgramID);
    glDeleteProgram(linesProgramID);
}

// --------------------------------------------------------------------------------
/*
    Draw the scene in the graphics window
*/
static int draw(std::string& error) {

    const GLuint phongProgramID = GetShader("phong");
    const GLuint linesProgramID = GetShader("lines");
    if (linesProgramID == 0 || phongProgramID == 0) {
        error += "shader not found: ";
        phongProgramID == 0 ? error += " phong " : " lines ";
        return -1;
    }

    // Clear the screen
    // not clearing the depth buffer can make back faces end up drawn over front ones and things will look inside out
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    // camera: projection uses the current aspect ratio, so resizing doesn't stretch the scene
    float aspect = framebufferHeight > 0 ? (float)framebufferWidth / (float)framebufferHeight : 1.0f;
    mat4 projection = perspective(radians(45.0f), aspect, 0.1f, 100.0f);
    mat4 view = lookAt(cameraPosition, cameraTarget, vec3(0.0f, 1.0f, 0.0f));

    // --- unlit lines: axes and light marker ---
    glUseProgram(linesProgramID);
    GLint mvpLoc = glGetUniformLocation(linesProgramID, "mvp");

    // axes are already in world space: model matrix = identity
    mat4 mvp = projection * view;
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, value_ptr(mvp));
    glBindVertexArray(axesVertexArrayID);
    glDrawArrays(GL_LINES, 0, 6);

    // light marker: scaled down and moved to the light's position
    mvp = projection * view * translate(mat4(1.0f), lightPosition) * scale(mat4(1.0f), vec3(0.1f));
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, value_ptr(mvp));
    glBindVertexArray(lightVertexArrayID);
    glDrawArrays(GL_LINES, 0, 6);

    // --- meshes: Phong shading ---
    glUseProgram(phongProgramID);

    // per-frame uniforms, the same for every model
    glUniformMatrix4fv(glGetUniformLocation(phongProgramID, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(phongProgramID, "projection"), 1, GL_FALSE, value_ptr(projection));
    glUniform3fv(glGetUniformLocation(phongProgramID, "lightPosition"), 1, value_ptr(lightPosition));
    glUniform3fv(glGetUniformLocation(phongProgramID, "lightColor"), 1, value_ptr(lightColor));
    glUniform1f(glGetUniformLocation(phongProgramID, "ambientStrength"), ambientStrength);
    glUniform3fv(glGetUniformLocation(phongProgramID, "lightAttenuation"), 1, value_ptr(lightAttenuation));
    glUniform3fv(glGetUniformLocation(phongProgramID, "viewPosition"), 1, value_ptr(cameraPosition));

    GLint modelLoc = glGetUniformLocation(phongProgramID, "model");
    GLint normalMatrixLoc = glGetUniformLocation(phongProgramID, "normalMatrix");

    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

    for (size_t i = 0; i < instances.size(); i++) {
        const Instance &instance = instances[i];
        const Model &model = models[instance.model];

        // fit first (center + normalize size), then the user's scale/rotation/translation
        mat4 modelMatrix = instance.ModelMatrix() * model.fit;
        mat3 normalMatrix = inverseTranspose(mat3(modelMatrix));

        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, value_ptr(modelMatrix));
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, value_ptr(normalMatrix));

        DrawModel(model, phongProgramID);
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // unbind the VAO safely
    glBindVertexArray(0);

    return 0;
}

// end of file
