/*
    An attempt at a very tiny Flight Simulator.

    Source material used as reference:
    https://www.di.ubi.pt/~agomes/cg/assignments/02-assignment.pdf
    https://www.fileformat.info/format/material/

    Author: Hugo Carvalho <a36295@ubi.pt>
    Email: hugo.carvalho@ubi.pt
*/

#ifndef SIMULATOR_HPP
#define SIMULATOR_HPP

// Include OpenGL related headers
#include <glm/glm.hpp>                  // GLM header file

// 
enum Phase {
    LINED_UP,
    TAKEOFF_ROLL,
    ROTATION,
    CLIMB,
};

struct Aircraft {
    // main wheel contact point, world space, meters
    glm::vec3 position;
    // m/s, along the nose
    float speed;
    // radians, nose up > 0 
    float pitch;
    // between 0 and 1
    float throttle;
    bool brakes;
    Phase phase;
};

/*
    Run the simulation program.
*/
int RunSimulator(GLFWwindow *window);

#endif