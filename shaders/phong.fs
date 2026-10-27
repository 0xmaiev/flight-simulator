#version 330 core

// Phong *shading*: lighting is computed per fragment from the interpolated normal
// (computing it per vertex and interpolating the color would be Gouraud shading).
// Phong *reflection model*: color = ambient + diffuse + specular.

// Interpolated values from the vertex shader
in vec3 fragPosition;
in vec3 fragNormal;
in vec2 fragUV;

// Point light, in world space
uniform vec3 lightPosition;
uniform vec3 lightColor;
uniform float ambientStrength;
// distance attenuation 1 / (constant + linear * d + quadratic * d^2)
uniform vec3 lightAttenuation;

// Camera position, in world space (needed for the specular term)
uniform vec3 viewPosition;

// Material (per submesh)
uniform vec3 materialKa;
uniform vec3 materialKd;
uniform vec3 materialKs;
uniform float materialShininess;
uniform bool hasDiffuseTexture;
uniform sampler2D diffuseTexture;

// Output data
out vec3 color;

void main() {
	// surface color: Kd, multiplied by the texture when there is one
	vec3 albedo = materialKd;
	if (hasDiffuseTexture) {
		albedo *= texture(diffuseTexture, fragUV).rgb;
	}

	// interpolation shortens the normal, so re-normalize per fragment
	vec3 N = normalize(fragNormal);
	// two-sided lighting: for back faces (e.g. the inside of an open mesh) flip the normal
	if (!gl_FrontFacing) {
		N = -N;
	}

	// point light: the direction to the light changes across the surface
	vec3 toLight = lightPosition - fragPosition;
	float distance = length(toLight);
	vec3 L = toLight / distance;
	vec3 V = normalize(viewPosition - fragPosition);
	vec3 R = reflect(-L, N);

	float attenuation = 1.0 / (lightAttenuation.x +
	                           lightAttenuation.y * distance +
	                           lightAttenuation.z * distance * distance);

	// ambient: constant fill light so unlit sides aren't pure black
	vec3 ambient = ambientStrength * materialKa * albedo;

	// diffuse (Lambert): brightest where the surface faces the light
	float NdotL = max(dot(N, L), 0.0);
	vec3 diffuse = NdotL * albedo * lightColor;

	// specular: highlight where the light reflects towards the camera;
	// no highlight on surfaces facing away from the light
	float spec = 0.0;
	if (NdotL > 0.0) {
		spec = pow(max(dot(V, R), 0.0), materialShininess);
	}
	vec3 specular = spec * materialKs * lightColor;

	color = ambient + attenuation * (diffuse + specular);
}
