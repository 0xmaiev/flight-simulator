
#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <common/scene.hpp>

/*
    Rotates the selected instance around a *world* axis.

    Multiplying the new rotation on the left applies it after the existing one,
    i.e. around the fixed world axes, so dragging right always spins the object
    the same way on screen no matter how it's already rotated.
*/
void RotateSelectedAroundWorldAxis(Instances& instances, const size_t selected,
    const float angle, const glm::vec3 &axis);

/*
    Rotates the selected instance around a *local* axis.

    Multiplying the new rotation on the right applies it before the existing one,
    i.e. around the object's own, already rotated axes. This allows mimicking real flight 
    dynamics where pulling back on the stick pitches the nose "up" relative to the 
    pilot, regardless of whether the aircraft is inverted or banked.
*/
void RotateSelectedAroundLocalAxis(Instances& instances, const size_t selected,
    const float angle, const glm::vec3 &axis);

#endif
