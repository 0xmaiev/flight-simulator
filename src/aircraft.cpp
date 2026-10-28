
#include <stdio.h>

#include <glm/gtc/type_ptr.hpp>         // GLM type pointers

#include <common/aircraft.hpp>

static const float MAX_ACCELERATION   = 1.4f;                  // m/s^2 at full throttle
static const float BRAKE_DECELERATION = 3.0f;                  // m/s^2
static const float ROTATION_SPEED     = 28.0f;                 // m/s, ~55 kt
static const float PITCH_RATE         = glm::radians(5.0f);    // rad/s at full elevator
static const float MAX_PITCH          = glm::radians(15.0f);

static const float FLIGHT_SPEED       = 30.0f;
static const float FLIGHT_PITCH       = glm::radians(5.0f);

static const float CLIMB_PITCH        = glm::radians(9.0f);

static const float OBSTACLE_HEIGHT    = 15.0f;                 // 15m


void AutoPilot(Aircraft &a) {

}

/*
    Fly the takeoff by setting the same controls the keyboard sets.
*/
void AutoTakeOff(Aircraft &a) {
    a.brakes   = false;
    float throttle = 0.0f;
    if (a.phase < 4) {
        throttle = 1.0f;
    }
    a.throttle = throttle;
    // at Vr raise the nose to the climb attitude; releasing the elevator holds the pitch
    a.elevator = (a.speed >= ROTATION_SPEED && a.pitch < CLIMB_PITCH) ? 1.0f : 0.0f;
}

/*
    State transitions to help debug phases and maybe do some other stuff with autopilot
*/
void updatePhase(Aircraft &a) {
    Phase previous = a.phase;
    switch (a.phase) {
        case LINED_UP:         if (!a.brakes && a.throttle > 0.0f)  a.phase = TAKEOFF_ROLL;     break;
        case TAKEOFF_ROLL:     if (a.speed >= ROTATION_SPEED)       a.phase = ROTATION;         break;
        case ROTATION:         if (a.position.y > 0.0f)             a.phase = CLIMB;            break;
        case CLIMB:            if (a.position.y >= OBSTACLE_HEIGHT) a.phase = FLIGHT; break;
        case FLIGHT: break;
    }
    if (a.phase != previous)
        fprintf(stdout, "phase %d at z = %.0f m, %.1f m/s\n", a.phase, a.position.z, a.speed);
}

void UpdateAircraft(Aircraft &a, float dt) {
    bool airborne = a.position.y > 0.0f;

    // along the runway: throttle pushes, brakes hold back
    float acceleration = a.throttle * MAX_ACCELERATION - (a.brakes ? BRAKE_DECELERATION : 0.0f);
    a.speed = glm::max(0.0f, a.speed + acceleration * dt);

    // below Vr there isn't enough airflow over the elevator to lift the nose, so it settles back
    float elevator = a.speed >= ROTATION_SPEED ? a.elevator : -1.0f;
    a.pitch = glm::clamp(a.pitch + elevator * PITCH_RATE * dt, 0.0f, MAX_PITCH);

    // lift grows with speed squared, so the slower it goes the more pitch the wing needs
    // (FLIGHT_PITCH at FLIGHT_SPEED); whatever pitch is left over becomes the climb angle
    float needed = FLIGHT_PITCH * (FLIGHT_SPEED * FLIGHT_SPEED) / glm::max(a.speed * a.speed, 1.0f);
    float climb  = a.pitch - needed;

    float deltaY, deltaZ;

    // the runway holds it, can climb but not sink
    if (!airborne) climb = glm::max(climb, 0.0f);

    a.position.y = glm::max(0.0f, a.position.y + a.speed * glm::sin(climb) * dt);   // up is +Y
    a.position.z += a.speed * glm::cos(climb) * dt;                                 // the nose points +Z

    // Update the phase
    updatePhase(a);
}