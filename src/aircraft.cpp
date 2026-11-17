
#include <stdio.h>

#include <glm/gtc/type_ptr.hpp>         // GLM type pointers

#include <common/aircraft.hpp>

// physics
static const float GRAVITY            = 9.81f;                 // m/s^2
static const float DRAG               = 0.0005f;               // 1/m: top speed sqrt(1.4 / 0.0005) = 53 m/s
static const float MIN_PITCH          = glm::radians(-10.0f);  // in the air; on the ground the nosewheel stops it at 0
static const float MAX_BANK           = glm::radians(25.0f);   // at full steering
static const float ROLL_RATE          = glm::radians(15.0f);   // rad/s
static const float GROUND_TURN_RATE   = glm::radians(15.0f);   // rad/s at full steering
static const float TAXI_SPEED         = 5.0f;                  // m/s, below this the nosewheel turns it less


static const float MAX_ACCELERATION   = 1.4f;                  // m/s^2 at full throttle
static const float BRAKE_DECELERATION = 3.0f;                  // m/s^2
static const float ROTATION_SPEED     = 28.0f;                 // m/s, ~55 kt
static const float PITCH_RATE         = glm::radians(5.0f);    // rad/s at full elevator
static const float MAX_PITCH          = glm::radians(15.0f);

static const float FLIGHT_SPEED       = 30.0f;
static const float FLIGHT_PITCH       = glm::radians(5.0f);

static const float CLIMB_PITCH        = glm::radians(9.0f);

static const float OBSTACLE_HEIGHT    = 15.0f;                 // 15m

// autopilot
static const float LOOKAHEAD          = 300.0f;                // m: steers at a point this far ahead on the leg
static const float TURN_DISTANCE      = 350.0f;                // m: starts the next leg this early, so the turn ends on it
static const float HEADING_GAIN       = 3.0f;                  // steering per radian of heading error
static const float ALTITUDE_GAIN      = 0.01f;                 // radians of climb per meter below the leg's height
static const float MAX_CLIMB          = glm::radians(4.0f);
static const float MAX_DESCENT        = glm::radians(4.0f);
static const float SPEED_GAIN         = 0.2f;                  // throttle per m/s too slow
static const float FLARE_HEIGHT       = 8.0f;                  // m
static const float FLARE_CLIMB        = glm::radians(-0.5f);   // the gentle sink it touches down with


/*
    Pitch the wing needs to carry the aircraft at this speed: lift grows with speed squared.
*/
static float liftPitch(float speed) {
    return FLIGHT_PITCH * (FLIGHT_SPEED * FLIGHT_SPEED) / glm::max(speed * speed, 1.0f);
}

/*
    The same angle, between -pi and pi.
*/
static float wrapAngle(float angle) {
    return glm::atan(glm::sin(angle), glm::cos(angle));
}

/*
    Fly the route: take off, follow each leg, then land on the last one.
*/
void AutoPilot(Aircraft &a, Route &route) {
    // a reset aircraft starts the route again
    if (a.phase == LINED_UP) route.leg = 1;

    // the leg, flattened onto the ground
    const Waypoint &from = route.waypoints[route.leg - 1];
    const Waypoint &to   = route.waypoints[route.leg];
    glm::vec2 start(from.position.x, from.position.z), end(to.position.x, to.position.z);
    glm::vec2 here(a.position.x, a.position.z);
    float length = glm::distance(start, end);
    glm::vec2 direction = (end - start) / length;
    // how far along the leg it has got
    float along = glm::dot(here - start, direction);

    // switch legs early so the turn ends on the new leg; the last leg runs on past the touchdown point
    bool last = route.leg + 1 == route.waypoints.size();
    if (!last && along > length - TURN_DISTANCE) {
        route.leg++;
        fprintf(stdout, "leg: %s\n", route.waypoints[route.leg].name);
        return;
    }

    // steer at a point a little further along the leg: when it's off the line, that point pulls it back on
    glm::vec2 carrot = start + direction * (along + LOOKAHEAD);
    float error = wrapAngle(glm::atan(carrot.x - here.x, carrot.y - here.y) - a.yaw);
    // the carrot to the left (error > 0) needs a left turn
    a.steering = glm::clamp(-HEADING_GAIN * error, -1.0f, 1.0f);

    // takeoff: full power, and at Vr raise the nose to the climb attitude
    if (a.phase < FLIGHT) {
        a.brakes   = false;
        a.throttle = 1.0f;
        a.elevator = (a.speed >= ROTATION_SPEED && a.pitch < CLIMB_PITCH) ? 1.0f : 0.0f;
        return;
    }

    // landed: power off and brake, still steering along the runway
    if (a.phase >= BRAKING) {
        a.brakes   = true;
        a.throttle = 0.0f;
        a.elevator = 0.0f;
        return;
    }

    // the leg's own slope, plus a correction towards the height it should have by now
    float slope = glm::atan((to.position.y - from.position.y) / length);
    float climb;
    if (last && a.position.y < FLARE_HEIGHT) {
        // flare: power off, easing from the glide path to a gentle sink
        climb = glm::mix(FLARE_CLIMB, slope, a.position.y / FLARE_HEIGHT);
        a.throttle = 0.0f;
    } else {
        float height = glm::mix(from.position.y, to.position.y, glm::clamp(along / length, 0.0f, 1.0f));
        climb = glm::clamp(slope + ALTITUDE_GAIN * (height - a.position.y), -MAX_DESCENT, MAX_CLIMB);
        // the throttle that balances drag and gravity on that climb, plus a nudge towards the leg's speed
        float hold = (DRAG * a.speed * a.speed + GRAVITY * glm::sin(climb)) / MAX_ACCELERATION;
        a.throttle = glm::clamp(hold + SPEED_GAIN * (to.speed - a.speed), 0.0f, 1.0f);
    }
    a.brakes = false;

    // the pitch for that climb is UpdateAircraft's lift model backwards
    float pitch = climb + liftPitch(a.speed);
    a.elevator = glm::clamp((pitch - a.pitch) / glm::radians(2.0f), -1.0f, 1.0f);
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
        case CLIMB:            if (a.position.y >= OBSTACLE_HEIGHT) a.phase = FLIGHT;           break;
        case FLIGHT:           if (a.position.y <= 0.0f)            a.phase = BRAKING;          break;
        case BRAKING:          if (a.speed <= 0.0f)                 a.phase = STOPPED;          break;
        case STOPPED:  break;
    }
    if (a.phase != previous)
        fprintf(stdout, "phase %d at z = %.0f m, %.1f m/s\n", a.phase, a.position.z, a.speed);
}

void UpdateAircraft(Aircraft &a, float dt) {
    bool onGround = a.position.y <= 0.0f;

    // throttle pushes; drag and (when climbing) gravity hold it back; the wheel brakes only work on the ground
    float acceleration = a.throttle * MAX_ACCELERATION - DRAG * a.speed * a.speed
                       - GRAVITY * glm::sin(a.climb) - (a.brakes && onGround ? BRAKE_DECELERATION : 0.0f);
    a.speed = glm::max(0.0f, a.speed + acceleration * dt);

    // below Vr there isn't enough airflow over the elevator to lift the nose, so it settles back
    float elevator = a.speed >= ROTATION_SPEED ? a.elevator : -1.0f;
    a.pitch = glm::clamp(a.pitch + elevator * PITCH_RATE * dt, onGround ? 0.0f : MIN_PITCH, MAX_PITCH);

    // in the air steering rolls the wings towards steering * MAX_BANK; on the ground they stay level
    float bank = onGround ? 0.0f : a.steering * MAX_BANK;
    a.bank += glm::clamp(bank - a.bank, -ROLL_RATE * dt, ROLL_RATE * dt);

    // on the ground the nosewheel turns it once it's rolling; in the air the bank does (a coordinated turn)
    float turnRate = onGround ? a.steering * GROUND_TURN_RATE * glm::min(a.speed / TAXI_SPEED, 1.0f)
                              : GRAVITY * glm::tan(a.bank) / glm::max(a.speed, 1.0f);
    // turning right is clockwise seen from above, so yaw goes down
    a.yaw -= turnRate * dt;

    // whatever pitch the wing doesn't need becomes the climb angle; the runway stops it sinking
    a.climb = a.pitch - liftPitch(a.speed);
    if (onGround) a.climb = glm::max(a.climb, 0.0f);

    glm::vec3 direction(glm::cos(a.climb) * glm::sin(a.yaw),
                        glm::sin(a.climb),
                        glm::cos(a.climb) * glm::cos(a.yaw));
    a.position += direction * a.speed * dt;
    a.position.y = glm::max(a.position.y, 0.0f);

    updatePhase(a);
}