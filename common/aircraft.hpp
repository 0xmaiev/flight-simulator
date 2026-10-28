
#ifndef AIRCRAFT_HPP
#define AIRCRAFT_HPP

// Include OpenGL related headers
#include <glm/glm.hpp>                  // GLM header file

// Phases for an autopilot state machine
enum Phase {
    LINED_UP,
    TAKEOFF_ROLL,
    ROTATION,
    CLIMB,
    FLIGHT,
    LINING_UP,
    DESCENT,
    BRAKING,
    STOPPED,
};

struct Aircraft {
    // main wheel contact point, world space, meters
    glm::vec3 position = glm::vec3(0.0f);
    // m/s, along the nose
    float speed        = 0.0f;
    // radians, nose up > 0 
    float pitch        = 0.0f;
    // between 0 and 1
    float throttle     = 0.0f;
    
    float elevator     = 0.0f;
    bool brakes        = true;
    Phase phase        = LINED_UP;
};

/*
    Update the aircraft data. 
*/
void AutoPilot(Aircraft &a);

/*
    Update the aircraft data. 
*/
void AutoTakeOff(Aircraft &a);

/*
    Update the aircraft data. 
*/
void UpdateAircraft(Aircraft &a, float dt);

#endif