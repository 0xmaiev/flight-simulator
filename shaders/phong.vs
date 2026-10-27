#version 330 core

// Input vertex data; locations match VERTEX_ATTRIB_* in viewer.hpp
layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 vertexUV;

// Transforms: object -> world -> camera -> clip space
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
// transpose(inverse(mat3(model))), computed once on the CPU instead of per vertex:
// keeps normals perpendicular to the surface under non-uniform scaling
uniform mat3 normalMatrix;

// Output fragment data, interpolated across each triangle
out vec3 fragPosition;  // world space
out vec3 fragNormal;    // world space, NOT normalized after interpolation
out vec2 fragUV;

void main() {
	vec4 worldPosition = model * vec4(vertexPosition, 1.0);

	fragPosition = worldPosition.xyz;
	fragNormal = normalMatrix * vertexNormal;
	fragUV = vertexUV;

	gl_Position = projection * view * worldPosition;
}
