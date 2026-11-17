#version 330 core

layout(location = 0) in vec3 position;

out vec3 texCoordDir;

uniform mat4 projection;
uniform mat4 view;

void main() {
    // negate x because we changed axes to have Y up
    texCoordDir = vec3(-position.x, position.y, position.z); 

    mat4 staticView = mat4(mat3(view));

    vec4 pos = projection * staticView * vec4(position, 1.0);

    gl_Position = pos.xyww;
}