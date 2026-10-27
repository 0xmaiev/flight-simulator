/*
    It renders the Table mesh.

    Loads an .OBJ (+ .MTL + textures), lights it with a point light using Phong
    shading, and lets the user rotate/translate it with the mouse and keyboard.

    Controls:
        left mouse drag     rotate
        right mouse drag    translate X/Y
        mouse scroll        translate Z
        arrow keys          translate X/Y
        Page Up / Down      translate Z
        A / D               rotate around Y (yaw)
        W / S               rotate around X (pitch)
        Q / E               rotate around Z (roll)
        R                   reset
        F                   toggle wireframe
        Esc                 quit

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
#include <common/shader.hpp>            // GLSL shader header file

GLuint phongProgramID;          // GLSL program for the meshes (Phong shading)
GLuint linesProgramID;          // GLSL program for the axes and light marker (unlit)

GLuint axesVertexArrayID;       // Vertex array object for the X/Y/Z axes
GLuint axesVertexBuffer;        // Vertex buffer object for the X/Y/Z axes
GLuint axesColorBuffer;         // color buffer object (CBO)

GLuint lightVertexArrayID;      // Vertex array object for the light marker
GLuint lightVertexBuffer;       // Vertex buffer object for the light marker
GLuint lightColorBuffer;        // color buffer object (CBO)

GLint windowWidth = 800;
GLint windowHeight = 800;
GLint framebufferWidth = 800;   // in pixels; differs from the window size on HiDPI screens
GLint framebufferHeight = 800;
GLFWwindow* window;             // graphics window of this program

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
void framebufferSizeCallback(GLFWwindow* window, int width, int height);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

void processKeyboard(float deltaTime);
void rotateSelected(float angle, const vec3 &axis);

bool transferDataToGPUMemory(const vector<obj_t> &objects);
void cleanupDataFromGPU(void);
void draw(void);

//--------------------------------------------------------------------------------


int main( void )
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

    // Open a window
    window = glfwCreateWindow( windowWidth, windowHeight, "Mesh Renderer", NULL, NULL);
    if (!window) {
        fprintf(stderr, "failed to create opengl window\n");
        glfwTerminate();
        return 1;
    }

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
    if ( !LoadObj("assets/objects/Table/Table.obj", &obj) ) {
        fprintf(stderr, "failed to load obj\n");
        return 1;
    }
    PrintObj(obj);
    // push onto the vector
    objects.push_back(obj);

    // transfer my data (shaders, axes, models, textures) to GPU side
    if (!transferDataToGPUMemory(objects)) {
        glfwTerminate();
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

    // render scene for each frame
    do {
        // time since the last frame, so key movement speed doesn't depend on the frame rate
        double now = glfwGetTime();
        float deltaTime = (float)(now - lastTime);
        lastTime = now;

        // looking for events (calls the callbacks)
        glfwPollEvents();
        // keys held down
        processKeyboard(deltaTime);
        // drawing callback
        draw();
        // Swap buffers
        glfwSwapBuffers(window);

    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
          glfwWindowShouldClose(window) == GL_FALSE );

    // Cleanup VAO, VBOs, and shaders from GPU
    cleanupDataFromGPU();

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

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
        rotateSelected(dx * mouseRotateSpeed, vec3(0.0f, 1.0f, 0.0f));
        rotateSelected(dy * mouseRotateSpeed, vec3(1.0f, 0.0f, 0.0f));
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
    the same way on screen no matter how it's already rotated. (Multiplying on
    the right would rotate around the object's own, already rotated, axes.)
*/
void rotateSelected(float angle, const vec3 &axis) {
    Instance &instance = instances[selected];
    instance.orientation = normalize(angleAxis(angle, axis) * instance.orientation);
}

/*
    Keys that act while held down, polled once per frame.
*/
void processKeyboard(float deltaTime) {
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
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) rotateSelected(-turn, vec3(0.0f, 1.0f, 0.0f));
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) rotateSelected( turn, vec3(0.0f, 1.0f, 0.0f));
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) rotateSelected(-turn, vec3(1.0f, 0.0f, 0.0f));
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) rotateSelected( turn, vec3(1.0f, 0.0f, 0.0f));
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) rotateSelected( turn, vec3(0.0f, 0.0f, 1.0f));
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) rotateSelected(-turn, vec3(0.0f, 0.0f, 1.0f));
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
bool transferDataToGPUMemory(const vector<obj_t> &objects)
{
    // Create and compile our GLSL programs from the shaders
    phongProgramID = LoadShaders("shaders/phong.vs", "shaders/phong.fs");
    linesProgramID = LoadShaders("shaders/lines.vs", "shaders/lines.fs");
    if (phongProgramID == 0 || linesProgramID == 0) {
        fprintf(stderr, "failed to load shaders\n");
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

    glDeleteProgram(phongProgramID);
    glDeleteProgram(linesProgramID);
}

// --------------------------------------------------------------------------------
/*
    Draw the scene in the graphics window
*/
void draw(void) {
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
}


// end of file
