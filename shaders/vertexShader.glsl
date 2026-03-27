#version 330 core
layout (location = 0) in vec2 vertexPos;
layout (location = 1) in vec2 instancePos;
layout (location = 2) in vec2 instanceScale;
layout (location = 3) in float instanceRotation;
layout (location = 4) in float instanceSpeed;
layout (location = 5) in float instanceDensity;

out float speedSq;
out float density;

uniform mat4 view;
uniform mat4 projection;

void main() {
    float c = cos(instanceRotation);
    float s = sin(instanceRotation);
    mat2 rot = mat2(c, -s, s, c);
    vec2 worldPos = rot * (vertexPos * instanceScale) + instancePos;
    gl_Position = projection * view * vec4(worldPos, 0.0, 1.0);
    speedSq = instanceSpeed;
    density = instanceDensity;
}