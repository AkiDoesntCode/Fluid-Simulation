#version 330 core
out vec4 FragColor;
in float speedSq;
in float density;

void main()
{
    float r = 0.3f;
    float g = 1.0 - exp(-density * 0.05f);
    float b = 0.75f + 0.25f * exp(-speedSq / 100);
    FragColor = vec4(r, 0.2f * g, b, 1.0f);
}