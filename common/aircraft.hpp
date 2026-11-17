
#ifndef AIRCRAFT_HPP
#define AIRCRAFT_HPP

#include <vector>

// Include OpenGL related headers
#include <glm/glm.hpp>                  // GLM header file

// Phases for an autopilot state machine
enum Phase {
    LINED_UP,
    TAKEOFF_ROLL,
    ROTATION,
    CLIMB,
    FLIGHT,
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
    // radians about +Y: 0 = nose along +Z, > 0 = turned left
    float yaw          = 0.0f;
    // radians, right wing down > 0
    float bank         = 0.0f;
    // radians, the angle it's moving at above the horizon
    float climb        = 0.0f;
    // between 0 and 1
    float throttle     = 0.0f;
    // between -1 and 1
    float elevator     = 0.0f;
    // -1 is left +1 is right
    float steering     = 0.0f;
    bool brakes        = true;
    Phase phase        = LINED_UP;
};


struct Waypoint {
    const char *name;       // the leg that ends here, for the log
    glm::vec3 position;     // world space; y is the height to pass it at
    float speed;            // m/s to fly the leg towards it
};

struct Route {
    std::vector<Waypoint> waypoints;    // the first is where the takeoff starts, the last the touchdown point
    size_t leg = 1;                     // flying from waypoints[leg - 1] to waypoints[leg]
};

/*
    Fly the route: take off, follow each leg, then land on the last one.
*/
void AutoPilot(Aircraft &a, Route &route);

/*
    Update the aircraft data. 
*/
void UpdateAircraft(Aircraft &a, float dt);

#endif