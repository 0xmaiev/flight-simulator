/*
    An attempt at a very tiny flight simulator.

    Source material used as reference:

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
#include <common/simulator.hpp>         // very tiny simulator header file
#include <common/model.hpp>             // very tiny OBJ model header file
#include <common/scene.hpp>             // very tiny scene header file
#include <common/viewer.hpp>            // very tiny OBJ viewer header file
#include <common/shader.hpp>            // GLSL shader header file
#include <common/helpers.hpp>           // reusable helpers between OBJ viewer and simulator

struct SimulatorState {
    Viewport* viewport;
    Camera* camera;
    Scene* scene;
    PointLight* light;
    Aircraft* aircraft;
    bool rotating       = false;    // left mouse button held
    bool panning        = false;    // right mouse button held
    double lastCursorX  = 0.0;
    double lastCursorY  = 0.0;

    SimulatorState(Viewport* vp, Camera* camera, Scene* scene,
        PointLight* light, Aircraft* aircraft): 
        viewport(vp),
        camera(camera),
        scene(scene),
        light(light),
        aircraft(aircraft)
    {};
};


// --------------------------------------------------------------------------------
/*
    Listing function prototypes
*/
// --------------------------------------------------------------------------------

// callbacks

// todo: move framebufferSizeCallback to reusable namespace
// keep all others in this source file because they are specific viewer controls
static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

static bool transferDataToGPUMemory(SimulatorState& state, const vector<obj_t>& objects, std::string& error);
static void cleanupDataFromGPU(SimulatorState& state);

static void processKeyboard(GLFWwindow *window, float deltaTime);
static int draw(SimulatorState& state, std::string& error);


// --------------------------------------------------------------------------------
/*
    Callbacks
*/

static void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    // get window attached context and perform null check
    SimulatorState* state = static_cast<SimulatorState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;
    
    Viewport* viewport = state->viewport;
    if (viewport == nullptr) return;

    // remembered so the projection's aspect ratio follows the window (see draw)
    viewport->framebuffer_height = height;
    viewport->framebuffer_width = width;
    glViewport(0, 0, width, height);
}

/*
    One-shot keys: called once per press. Keys that are held down
    are polled every frame in processKeyboard instead.
*/
static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    // get window attached context and perform null check
    SimulatorState* state = static_cast<SimulatorState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;
    
    Aircraft* aircraft = state->aircraft;
    if (aircraft == nullptr) return;
    if (action != GLFW_PRESS) return;

    switch (key) {
        case GLFW_KEY_R: {
            // reset the aircraft position to the starting point placement
            //Instance &instance = instances[selected];
            //instance.position = vec3(0.0f);
            //instance.orientation = quat(1.0f, 0.0f, 0.0f, 0.0f);
            break;
        }
    }
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    // get window attached context and perform null check
    SimulatorState* state = static_cast<SimulatorState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        state->rotating = (action == GLFW_PRESS);
    } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        state->panning = (action == GLFW_PRESS);
    }

    // start measuring the drag from where the button was pressed
    glfwGetCursorPos(window, &state->lastCursorX, &state->lastCursorY);
}

static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    // get window attached context and perform null check
    SimulatorState* state = static_cast<SimulatorState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;

    // cursor movement since the last event, in pixels (y grows downwards)
    float dx = (float)(xpos - state->lastCursorX);
    float dy = (float)(ypos - state->lastCursorY);
    state->lastCursorX = xpos;
    state->lastCursorY = ypos;

    Aircraft* aircarft = state->aircraft;
    if (aircarft == nullptr) return;

    if (state->rotating) {
        // horizontal drag spins the CAMERA around the AIRCRAFT's vertical axis,
        // vertical drag tips it forwards/backwards, not the AIRCRAFT itself
        //RotateSelectedAroundLocalAxis(instances, selected,
        //    dx * MOUSE_ROTATE_SPEED, vec3(0.0f, 1.0f, 0.0f));
        //RotateSelectedAroundLocalAxis(instances, selected,
        //    dy * MOUSE_ROTATE_SPEED, vec3(1.0f, 0.0f, 0.0f));
    }
    if (state->panning) {
        //instances[selected].position.x += dx * MOUSE_MOVE_SPEED;
        //instances[selected].position.y -= dy * MOUSE_MOVE_SPEED;
    }
}

static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    // get window attached context and perform null check
    SimulatorState* state = static_cast<SimulatorState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;

    Aircraft* aircraft = state->aircraft;
    Camera* camera = state->camera;

    if (camera == nullptr || aircraft == nullptr) return;

    // The camera position is relative to the aircraft so instead of changing
    // the aircraft position directly we change the camera position

    // scrolling up brings the aircraft closer (+Z points towards the camera)
    camera->position.z += (float)yoffset * SCROLL_MOVE_SPEED;
}

/*
    Keys that act while held down, polled once per frame.
*/
static void processKeyboard(GLFWwindow *window, float deltaTime) {
    // get window attached context and perform null check
    SimulatorState* state = static_cast<SimulatorState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;
    
    Aircraft* aircraft = state->aircraft;
    
    if (aircraft == nullptr) return;

    float move = KEY_MOVE_SPEED* deltaTime;
    float turn = KEY_ROTATE_SPEED * deltaTime;

    // translate
    //if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)      instance.position.x -= move;
    //if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)     instance.position.x += move;
    //if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)      instance.position.y -= move;
    //if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)        instance.position.y += move;
    //if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS) instance.position.z -= move;
    //if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)   instance.position.z += move;

    // rotate
    //if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) // LEFT ROLL
    //    RotateSelectedAroundWorldAxis(instances, selected, -turn, vec3(0.0f, 1.0f, 0.0f));
    //if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) // RIGHT ROLL
    //    RotateSelectedAroundWorldAxis(instances, selected, turn, vec3(0.0f, 1.0f, 0.0f));
    //if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) // ENGINE ON / SPEED / TORQUE
    //    RotateSelectedAroundWorldAxis(instances, selected, -turn, vec3(1.0f, 0.0f, 0.0f));
    //if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) // BRAKES
    //   RotateSelectedAroundWorldAxis(instances, selected,  turn, vec3(1.0f, 0.0f, 0.0f));
    //if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
    //    RotateSelectedAroundWorldAxis(instances, selected,  turn, vec3(0.0f, 0.0f, 1.0f));
    //if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    //    RotateSelectedAroundWorldAxis(instances, selected, -turn, vec3(0.0f, 0.0f, 1.0f));
}



int RunSimulator(GLFWwindow *window) {

    // set callbacks: resizing, and the GLFW mouse/keyboard input
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    // Clear background
    glClearColor(0.0f, 0.0f, 0.00f, 0.0f);

    const vector<std::string> paths = {
        "assets/objects/Runway/Runway.obj",
        "assets/objects/Cessna172SP/Cessna172SP.obj",
        "assets/objects/Cessna172SP/Cessna172SP-Parked.obj",
    };

    vector<obj_t> objects;

    for (std::string path : paths) {
        obj_t obj;

        // load table object
        if ( !LoadObj(path.c_str(), &obj) ) {
            fprintf(stderr, "failed to load obj\n");
            return 1;
        }
        // push onto the vector
        objects.push_back(obj);
    }
    
    std::string error;
    
    Scene scene;
    Camera camera;
    Viewport viewport;
    Aircraft aircraft;
    PointLight light;
    SimulatorState state { &viewport, &camera, &scene, &light, &aircraft };

    glfwSetWindowUserPointer(window, &state);

    // transfer my data (shaders, axes, models, textures) to GPU side
    if (!transferDataToGPUMemory(state, objects, error)) {
        glfwTerminate();
        if (!error.empty()) {
            fprintf(stderr, "%s\n", error.c_str());
        }
        return 1;
    }

    // the GPU has its own copy now: the parsed .OBJ data can be freed
    objects.clear();
    
    // the framebuffer can be bigger than the window (HiDPI), so ask for its real size
    glfwGetFramebufferSize(window, &viewport.framebuffer_width, &viewport.framebuffer_height);
    glViewport(0, 0, viewport.framebuffer_width, viewport.framebuffer_height);

    fprintf(stdout,
        "\ncontrols:\n");

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
        drawResult = draw(state, error);
        if (drawResult == -1) {
            break;
        }
        // Swap buffers
        glfwSwapBuffers(window);

    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
          glfwWindowShouldClose(window) == GL_FALSE );

    // Cleanup VAO, VBOs, and shaders from GPU
    cleanupDataFromGPU(state);

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

/*
    Create shaders, VAOs and VBOs and transfer them to graphics card memory
*/
static bool transferDataToGPUMemory(SimulatorState& state, const vector<obj_t> &objects, std::string &error)
{
    // Create and compile our GLSL programs from the shaders
    bool ret = LoadShaders("shaders", error);
    if (!ret) {
        return false;
    }

    Models& models = state.scene->models;
    Instances& instances = state.scene->instances;
    TextureCache& textures = state.scene->textures;

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
void cleanupDataFromGPU(SimulatorState& state) {
    Models& models = state.scene->models;
    TextureCache& textures = state.scene->textures;

    for (size_t i = 0; i < models.size(); i++) {
        DeleteModel(&models[i]);
    }
    DeleteTextures(&textures);

    const ShaderRegistry registry = GetShaders();

    for (std::pair<std::string, GLuint> shader : registry) {
        const std::string& name = shader.first;
        const GLuint& programID = shader.second;
        glDeleteProgram(programID);
    }
}


// --------------------------------------------------------------------------------
/*
    Draw the scene in the graphics window
*/
static int draw(SimulatorState& state, std::string& error) {
    const GLuint phongProgramID = GetShader("phong");
    if (phongProgramID == 0) {
        error += "shader not found: phong";
        return -1;
    }

    Scene* scene = state.scene;
    Viewport* viewport = state.viewport;
    Camera* camera = state.camera;
    PointLight* light = state.light;
    Aircraft* aircraft = state.aircraft;

    // Clear the screen
    // not clearing the depth buffer can make back faces end up drawn over front ones and things will look inside out
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    // camera: projection uses the current aspect ratio, so resizing doesn't stretch the scene
    float aspect = viewport->framebuffer_height > 0 ?
        (float)viewport->framebuffer_width /(float)viewport->framebuffer_height : 1.0f;
    mat4 projection = perspective(radians(45.0f), aspect, 0.1f, 100.0f);
    mat4 view = lookAt(camera->position, camera->target, vec3(0.0f, 1.0f, 0.0f));

    // --- meshes: Phong shading ---
    glUseProgram(phongProgramID);

    // per-frame uniforms, the same for every model
    glUniformMatrix4fv(glGetUniformLocation(phongProgramID, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(phongProgramID, "projection"), 1, GL_FALSE, value_ptr(projection));
    glUniform3fv(glGetUniformLocation(phongProgramID, "lightPosition"), 1, value_ptr(light->position));
    glUniform3fv(glGetUniformLocation(phongProgramID, "lightColor"), 1, value_ptr(light->color));
    glUniform1f(glGetUniformLocation(phongProgramID, "ambientStrength"), light->ambient_strength);
    glUniform3fv(glGetUniformLocation(phongProgramID, "lightAttenuation"), 1, value_ptr(light->attenuation));
    glUniform3fv(glGetUniformLocation(phongProgramID, "viewPosition"), 1, value_ptr(camera->position));

    GLint modelLoc = glGetUniformLocation(phongProgramID, "model");
    GLint normalMatrixLoc = glGetUniformLocation(phongProgramID, "normalMatrix");

    for (size_t i = 0; i < scene->instances.size(); i++) {
        const Instance &instance = scene->instances[i];
        const Model &model = scene->models[instance.model];

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