
#ifndef SCENE_HPP
#define SCENE_HPP

#include <common/model.hpp>

// Module defaults
static const GLint DEFAULT_FRAMEBUFFER_WIDTH    = 800;
static const GLint DEFAULT_FRAMEBUFFER_HEIGHT   = 800;
static const GLfloat DEFAULT_AMBIENT_STRENGTH   = 0.15f;

/*
    One placement of a Model in the scene - what the mouse/keyboard move.
    Several instances can share the same Model (e.g. four tables in a room).
*/
struct Instance {
    // index into the models vector; an index (unlike a Model*) stays valid
    // when that vector grows and reallocates
    size_t model;
    glm::vec3 position      = glm::vec3(0.0f);
    // rotation as a quaternion: incremental rotations compose without gimbal lock
    glm::quat orientation   = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // identity (w, x, y, z)
    float scale             = 1.0f;

    Instance(size_t m): model(m) {}

    /*
        Model matrix M = T * R * S: applied right to left, a vertex is scaled first,
        then rotated around the origin, then translated to `position`.
    */
    glm::mat4 ModelMatrix() const {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 R = glm::mat4_cast(orientation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
        return T * R * S;
    }
};

// Instances of models loaded, each model can have more than one instance. 
typedef std::vector<Instance> Instances;

/*
    The contents of a scene, it's object models, textures and instances.
    This is pretty much all that is shared between viewer and simulator.
*/
struct Scene {
    // loaded models, can have more than one instance each
    Models models;
    // object instances vector
    Instances instances;
    // texture cache
    TextureCache textures;
};

/*
    Point light in world space.
*/
struct PointLight {
    glm::vec3 position      = glm::vec3(0.8f, 1.0f, 1.5f);
    glm::vec3 color         = glm::vec3(1.0f, 1.0f, 1.0f);
    float ambient_strength  = DEFAULT_AMBIENT_STRENGTH;
    glm::vec3 attenuation   = glm::vec3(1.0f, 0.05f, 0.01f);
};

/*
    Where we look from, each mode moves in it's own way.
    Viewer has static camera, flight simulator has dynamic camera.
*/
struct Camera {
    glm::vec3 position  = glm::vec3(0.0f, 1.2f, 4.0f);
    glm::vec3 target    = glm::vec3(0.0f, 0.0f, 0.0f);

    glm::mat4 View() const;
};

/*
    Information about the current viewport.
*/
struct Viewport {
    GLint framebuffer_width     = DEFAULT_FRAMEBUFFER_WIDTH;
    GLint framebuffer_height    = DEFAULT_FRAMEBUFFER_HEIGHT;

    glm::mat4 Projection() const;
};


#endif