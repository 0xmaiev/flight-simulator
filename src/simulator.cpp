/*
    An attempt at a very tiny flight simulator.

    Source material used as reference:
    https://www.jakobmaier.at/posts/flight-simulation/
    https://www.programmingcreatively.com/opengl-tutorial-11-qs.php

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
#include <common/aircraft.hpp>         // very tiny aircraft header file
#include <common/simulator.hpp>         // very tiny simulator header file
#include <common/model.hpp>             // very tiny OBJ model header file
#include <common/scene.hpp>             // very tiny scene header file
#include <common/viewer.hpp>            // very tiny OBJ viewer header file
#include <common/shader.hpp>            // GLSL shader header file
#include <common/helpers.hpp>           // reusable helpers between OBJ viewer and simulator

// object paths
const vector<std::string> objectPaths = {
    "assets/objects/Runway/Runway.obj",
    "assets/objects/Cessna172SP/Cessna172SP.obj",
};

// skybox paths
const vector<std::string> skyboxPaths = {
    "assets/cubemaps/Cloudy/Cubemap_Sky_01-512x512.png",
    "assets/cubemaps/Cloudy/Cubemap_Sky_02-512x512.png",
    "assets/cubemaps/Cloudy/Cubemap_Sky_04-512x512.png",
    "assets/cubemaps/Cloudy/Cubemap_Sky_16-512x512.png",
    "assets/cubemaps/Cloudy/Cubemap_Sky_22-512x512.png",
};

// positions for the skybox VAO
float skyboxVertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

static const float DEFAULT_TIME_MUL         = 1.0f;
static const float TIME_MULTIPLIER_DELTA    = 0.5f;

struct SimulatorState {
    Viewport* viewport;
    Camera* camera;
    Scene* scene;
    PointLight* light;
    Aircraft* aircraft;

    int selectedSkyboxIdx       = 0; // skybox index
    std::string activeSkybox   = "";
    GLuint skyboxVao            = 0;
    GLuint skyboxVbo            = 0;

    bool logDebug               = false;    // log debug messages
    bool automaticTakeOff       = false;    // automatic takeoff procedure
    bool orbiting               = false;    // left mouse button held
    
    float timeMultiplier        = DEFAULT_TIME_MUL; // multiplier to "fast forward" time

    double lastCursorX          = 0.0;
    double lastCursorY          = 0.0;

    double cameraYaw            = 0.0;      // horizontal / x
    double cameraPitch          = 0.1;      // vertical / y - initially slightly above plane
    double cameraDistance       = 15.0;     // distance / z - initially behind plane

    SimulatorState(Viewport* vp, Camera* camera, Scene* scene,
        PointLight* light, Aircraft* aircraft): 
        viewport(vp),
        camera(camera),
        scene(scene),
        light(light),
        aircraft(aircraft)
    {};
};

enum {
    RUNWAY,
    AIRCRAFT,
    TERMINAL,
    PARKED_AIRCRAFT
};

// scene
static const glm::vec3 AIRCRAFT_START_POSITION = vec3(0.0f, 0.0f, -1230.0f);

// a left-hand circuit back onto the same runway (x -30..30, z -1830..1830): upwind along +Z,
// crosswind to +X, downwind along -Z, base back to the centreline, then 1500 m of final
// down a 3 degree glide path (1500 * tan(3) = 79 m high where it starts)
static const std::vector<Waypoint> CIRCUIT = {
    {"takeoff",   AIRCRAFT_START_POSITION,          35.0f},
    {"upwind",    vec3(   0.0f, 120.0f,   300.0f), 35.0f},
    {"crosswind", vec3( 800.0f, 120.0f,   300.0f), 35.0f},
    {"downwind",  vec3( 800.0f, 120.0f, -3000.0f), 35.0f},
    {"base",      vec3(   0.0f,  79.0f, -3000.0f), 33.0f},
    {"final",     vec3(   0.0f,   0.0f, -1500.0f), 32.0f},
};

// controls
static const float KEY_MOVE_SPEED           = 1.5f;     // world units per second
static const float KEY_ROTATE_SPEED         = 1.5f;     // radians per second
static const float MOUSE_ROTATE_SPEED       = 0.5f;     // radians per pixel
static const float MOUSE_MOVE_SPEED         = 0.005f;   // world units per pixel
static const float SCROLL_MOVE_SPEED        = 0.2f;     // world units per scroll step

// aircraft
static const float MAX_THROTTLE             = 1.0f;
static const float THROTTLE_DELTA           = 0.5f;
static const float DEFAULT_THROTTLE         = 0.0f;
// elevator
static const float ELEVATOR_DELTA           = 1.0f;
static const float DEFAULT_ELEVATOR         = 0.0f;
// turn
static const float TURN_DELTA               = 1.0f;
static const float DEFAULT_TURN             = 0.0f;

// camera pitch
static const double MIN_CAMERA_PITCH        = 0.01f;    // radians
static const double MAX_CAMERA_PITCH        = 1.45f;    // radians



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
void updateScene(SimulatorState& state);
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
        case GLFW_KEY_L: {
            state->logDebug = !state->logDebug;
            break;
        }
        case GLFW_KEY_B: {
            aircraft->brakes = !aircraft->brakes;
            fprintf(stdout, "aircraft brakes: %s\n", aircraft->brakes ? "true" : "false");
            break;
        }
        case GLFW_KEY_R: {
            // reset the aircraft position to the starting point placement
            *aircraft = Aircraft();
            aircraft->position = AIRCRAFT_START_POSITION;
            break;
        }
        case GLFW_KEY_P: {
            if (state->selectedSkyboxIdx == skyboxPaths.size() - 1) {
                state->selectedSkyboxIdx = 0;
            } else {
                state->selectedSkyboxIdx += 1;
            }
            state->activeSkybox = skyboxPaths[state->selectedSkyboxIdx];
            break;
        }
        case GLFW_KEY_T: {
            // automatic take off
            state->automaticTakeOff = !state->automaticTakeOff;
            fprintf(stdout, "automatic takeoff procedure: %s\n",
                state->automaticTakeOff ? "true" : "false");
            break;
        }
        case GLFW_KEY_RIGHT_SHIFT: {
            state->timeMultiplier = state->timeMultiplier 
                + (state->timeMultiplier * TIME_MULTIPLIER_DELTA); // fast forward time
            fprintf(stdout, "time multiplier: %.1f", state->timeMultiplier);
            break;
        }
        case GLFW_KEY_LEFT_SHIFT: {
            state->timeMultiplier = DEFAULT_TIME_MUL; // reset time
            fprintf(stdout, "time multiplier reset");
            break;
        }
    }
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    // get window attached context and perform null check
    SimulatorState* state = static_cast<SimulatorState*>(glfwGetWindowUserPointer(window));
    if (state == nullptr) return;

    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        state->orbiting = (action == GLFW_PRESS);
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
    Camera* camera = state->camera;
    if (aircarft == nullptr || camera == nullptr) return;

    if (state->orbiting) {
        // horizontal drag spins the CAMERA around the AIRCRAFT's vertical axis,
        // vertical drag tips it forwards/backwards, not the AIRCRAFT itself
        state->cameraYaw += glm::radians(dx * MOUSE_ROTATE_SPEED);
        state->cameraPitch += glm::radians(dy * MOUSE_ROTATE_SPEED);
        // not clamping the pitch makes the camera flip at the extremes
        state->cameraPitch = glm::clamp(state->cameraPitch, MIN_CAMERA_PITCH, MAX_CAMERA_PITCH);
        //fprintf(stdout, "camera offset angle: x %f y %f\n",
        //    state->cameraYaw, state->cameraPitch);
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
    state->cameraDistance += (float)yoffset * SCROLL_MOVE_SPEED;
    state->cameraDistance = glm::clamp(state->cameraDistance, 5.0, 100.0);
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

    if (glfwGetKey(window, GLFW_KEY_PAGE_UP) == GLFW_PRESS)
        aircraft->throttle += THROTTLE_DELTA * deltaTime; // more throttle
    if (glfwGetKey(window, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS)
        aircraft->throttle -= THROTTLE_DELTA * deltaTime; // less throttle
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
        aircraft->throttle += THROTTLE_DELTA * deltaTime; // more throttle
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
        aircraft->throttle -= THROTTLE_DELTA * deltaTime; // less throttle

    // clamp throttle
    aircraft->throttle = glm::clamp(aircraft->throttle, DEFAULT_THROTTLE, MAX_THROTTLE);

    // reset so nose up / down only happens when a key is pressed
    aircraft->elevator = DEFAULT_ELEVATOR;
    aircraft->steering = DEFAULT_TURN;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) // left
        aircraft->steering -= TURN_DELTA;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) // right
        aircraft->steering += TURN_DELTA;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) // yoke forward / nose down
        aircraft->elevator -= ELEVATOR_DELTA;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) // yoke backward / nose up
        aircraft->elevator += ELEVATOR_DELTA;
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
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

    // object paths and loading 
    vector<obj_t> objects;

    for (std::string path : objectPaths) {
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

    aircraft.position   = AIRCRAFT_START_POSITION;
    light.position      = vec3(100.0f, 400.0f, -1100.f);
    light.attenuation   = vec3(1.0f, 0.0f, 0.0f);

    // skybox paths and loading 
    for (std::string path : skyboxPaths) {
        // load cubemap
        if ( !LoadCubemapCross(path.c_str(), &scene.textures) ) {
            fprintf(stderr, "failed to load obj\n");
            return 1;
        }
    }

    SimulatorState state { &viewport, &camera, &scene, &light, &aircraft };
    state.activeSkybox = skyboxPaths[state.selectedSkyboxIdx];

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
        "\ncontrols:\n"
        "  left mouse drag      camera orbit aircraft\n"
        "  PAGE / ARROW UP      throttle on\n"
        "  PAGE / ARROW DOWN    throttle off\n"
        "  LEFT SHIFT           reset time\n"
        "  RIGHT SHIFT          fast forward time\n"
        "  W                    yoke forward / nose down\n"
        "  S                    yoke backward / nose up\n"
        "  T                    automatic take off\n"
        "  R                    reset aircraft position\n"
        "  P                    change skybox\n"
        "  L                    toggle log debug messages\n"
        "  Esc                  quit\n");

    double lastTime = glfwGetTime();
    int drawResult;
    Route route;
    route.waypoints = CIRCUIT;

    // render scene for each frame
    do {
        // time since the last frame, so key movement speed doesn't depend on the frame rate
        double now = glfwGetTime();
        float deltaTime = ((float)(now - lastTime) * state.timeMultiplier);
        lastTime = now;

        if (state.logDebug) {
            fprintf(stdout, "%.1f fps\n", 1.0f / deltaTime);
        }

        // looking for events (calls the callbacks)
        glfwPollEvents();

        // if autopilot then we need to act on behalf of processing keys
        // a simple key press can still disable autopilot to get back in control
        if (state.automaticTakeOff) {
            AutoPilot(aircraft, route);
        } else {
            // keys held down
            processKeyboard(window, deltaTime);
        }

        // update the aircraft
        UpdateAircraft(aircraft, deltaTime);
        // update the scene
        updateScene(state);

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
    
    // skybox VAO/VBO
    glGenVertexArrays(1, &state.skyboxVao);
    glBindVertexArray(state.skyboxVao);

    // 1st attribute buffer : vertices
    glGenBuffers(1, &state.skyboxVbo);
    glBindBuffer(GL_ARRAY_BUFFER, state.skyboxVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    // unbind
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
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

    // 1 unit = 1 m: the runway file is 2x too big, the Cessna 3x too small. The Cessna's
    // pivot is where its main wheels touch the ground, so its position is that contact
    // point and pitching tilts it about the main wheels, like a real rotation
    models[RUNWAY].fit      = scale(mat4(1.0f), vec3(0.5f));
    models[AIRCRAFT].fit    = scale(mat4(1.0f), vec3(3.0f))
                            * translate(mat4(1.0f), -vec3(0.0f, 0.005f, 0.42f));

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

/*
    We have to update the scene after processing events.
    The plane instance position and orientation can change,
    and with it so does the camera.
*/
void updateScene(SimulatorState& state) {
    Scene* scene = state.scene;
    Camera* camera = state.camera;
    Aircraft* aircraft = state.aircraft;

    if (scene == nullptr || camera == nullptr
        || aircraft == nullptr) return;

    Instance& plane = scene->instances[AIRCRAFT];
    plane.position = aircraft->position;
    // applied right to left: bank about the nose, pitch about the wings, then heading about world up
    plane.orientation = angleAxis(aircraft->yaw, vec3(0.0f, 1.0f, 0.0f))
                      * angleAxis(-aircraft->pitch, vec3(1.0f, 0.0f, 0.0f))
                      * angleAxis(aircraft->bank, vec3(0.0f, 0.0f, 1.0f));

    // if we simply did this it would be like moving the camera sideways and up on a flat wall
    // the wall stays `cameraDistance` behind the aircraft, so the distance would then be
    // sqrt(d^2, x^2, y^2) and it would grow further out as we try to rotate
    //camera->position    = aircraft->position + vec3(0.0f, 3.0f, -state.cameraDistance);
    //camera->position.x  += state.cameraYaw;
    //camera->position.y  += state.cameraPitch;

    // the position then becomes an offset from the target
                // around the plane
    vec3 offset(state.cameraDistance * cos(state.cameraPitch) * sin(state.cameraYaw),
                // up
                state.cameraDistance * sin(state.cameraPitch),
                // behind: the nose points +Z
            -state.cameraDistance * cos(state.cameraPitch) * cos(state.cameraYaw));
    camera->target   = aircraft->position + vec3(0.0f, 1.5f, 0.0f);
    camera->position = camera->target + offset;
    // the orbit is measured from behind the aircraft, so it turns with it (but doesn't bank)
    camera->position = camera->target + angleAxis(aircraft->yaw, vec3(0.0f, 1.0f, 0.0f)) * offset;
}


// --------------------------------------------------------------------------------
/*
    Draw the scene in the graphics window
*/
static int draw(SimulatorState& state, std::string& error) {
    const GLuint phongProgramID = GetShader("phong");
    const GLuint skyboxProgramID = GetShader("skybox");
    if (phongProgramID == 0 || skyboxProgramID == 0) {
        error += "shader not found: ";
        error += phongProgramID == 0 ? "phong " : "";
        error += skyboxProgramID == 0 ? "skybox " : "";
        return -1;
    }

    Scene* scene = state.scene;
    Viewport* viewport = state.viewport;
    Camera* camera = state.camera;
    PointLight* light = state.light;
    Aircraft* aircraft = state.aircraft;

    if (scene == nullptr || viewport == nullptr || camera == nullptr
        || light == nullptr || aircraft == nullptr) return -2;

    // Clear the screen
    // not clearing the depth buffer can make back faces end up drawn over front ones and things will look inside out
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT  );

    // camera: projection uses the current aspect ratio, so resizing doesn't stretch the scene
    float aspect = viewport->framebuffer_height > 0 ?
        (float)viewport->framebuffer_width /(float)viewport->framebuffer_height : 1.0f;
    mat4 projection = perspective(radians(45.0f), aspect, 0.5f, 15000.0f);
    mat4 view = lookAt(camera->position, camera->target, vec3(0.0f, 1.0f, 0.0f));
    
    // sky first, with depth testing off so everything drawn after covers it
    glUseProgram(skyboxProgramID);
    glUniformMatrix4fv(glGetUniformLocation(skyboxProgramID, "view"), 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(skyboxProgramID, "projection"), 1, GL_FALSE, value_ptr(projection));
    glActiveTexture(GL_TEXTURE0);
    GLuint activeSkybox = scene->textures[state.activeSkybox];
    glBindTexture(GL_TEXTURE_CUBE_MAP, activeSkybox);
    glBindVertexArray(state.skyboxVao);
    glDisable(GL_DEPTH_TEST);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glEnable(GL_DEPTH_TEST);
    
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