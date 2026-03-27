#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <math.h>
#include <random>
#include <algorithm>
#include <unistd.h>
#include "object.h"

// Global Variables (Mostly to fix screen ratio issues)
int screenSizeX = 800;
int screenSizeY = 600;
int scaleCoefficent = 100; //pixels per "unit"
float gravity = -9.8f * scaleCoefficent;
float particleRadius = 3.2f;
int particles = 6000;
bool frozen = false;

// Uniform grid for simulation
float smoothingRadius = 20.0f;
int gridSize = (int) smoothingRadius;
int cellsX = screenSizeX / gridSize; // 100
int cellsY = screenSizeY / gridSize; // 75
glm::vec3 gravityVec = glm::vec3(0.0f, gravity, 0.0f);
std::vector<std::vector<int>> grid(cellsX * cellsY, std::vector<int>());
float area;

// transform matricies
glm::mat4 view;
glm::mat4 projection;

unsigned int viewLoc;
unsigned int projLoc;
unsigned int modelLocation;
unsigned int shaderProgram;
unsigned int speedLoc;

// Simulation
float restitution = 0.2f;
float stiffness = (float) scaleCoefficent * 0.9f;
float correction = 0.001f;
std::vector<glm::vec2> ghostCell;
float mouseForce = 80.0f;

std::random_device seed;
std::mt19937 gen{seed()};
std::uniform_int_distribution<> distx{10, 790};
std::uniform_int_distribution<> disty{10, 590};

std::vector<glm::vec2> generateGhostCell()
{
    std::vector<glm::vec2> ghostPositions;
    int ghostCount = 80;
    int ghostPerRow = (int)sqrt(ghostCount);
    float ghostSpacing = gridSize / (float)ghostPerRow;

    for (int gx = 0; gx < ghostPerRow; gx++)
    {
        for (int gy = 0; gy < ghostPerRow; gy++)
        {
            ghostPositions.push_back(glm::vec2(
                gx * ghostSpacing,
                gy * ghostSpacing
            ));
        }
    }
    return ghostPositions;
}

// Callback Functions
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    screenSizeX = width;
    screenSizeY = height;
    cellsX = screenSizeX / gridSize;
    cellsY = screenSizeY / gridSize;
    area = 10.0f / (M_PI * smoothingRadius * smoothingRadius); // 2D normalization
    ghostCell = generateGhostCell();
    grid.assign(cellsX * cellsY, std::vector<int>());
    projection = glm::ortho(0.0f, (float) screenSizeX, 0.0f, (float) screenSizeY, -1.0f, 1.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glViewport(0, 0, width, height);
}

// Fluid Sim Interactons
void freezeAnimation()
{
    frozen = !frozen;
}

// Periphial Interactions
void processInput(GLFWwindow *window)
{
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    static bool fPrevPressed = false;
    bool fPressed = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (fPressed && !fPrevPressed)
        freezeAnimation();
    fPrevPressed = fPressed;
}

std::string readFile(const char* path)
{
    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Fluid Simulation Equations
float calculateDensity(float distance, float mass)
{
    float x = (smoothingRadius - distance) / smoothingRadius;
    return mass * x * x * x;
}

float calculateGradient(float distance, float mass)
{
    float x = (smoothingRadius - distance) / smoothingRadius;
    return mass * x * x * (1.0f / (smoothingRadius * distance));
}

// Grid Functions
void generateGrid(std::vector<std::unique_ptr<Particle>>& objectArray)
{
    for (int i = 0; i < objectArray.size(); i++)
    {
        glm::vec3 objectPos = objectArray[i]->position;
        int gridX = std::clamp((int)(objectPos.x / gridSize), 0, cellsX - 1);
        int gridY = std::clamp((int)(objectPos.y / gridSize), 0, cellsY - 1);

        int key = gridX + gridY * cellsX;
        grid[key].push_back(i);
    }
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    

    GLFWwindow* window = glfwCreateWindow(screenSizeX, screenSizeY, "Particle Simulation", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // For fragment shader blending
    //glEnable(GL_BLEND);
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // VSYNC (For Smoother Collisions)
    glfwSwapInterval(0);

    // Create vertex and fragment shaders
    std::string vertexCode = readFile("src/shaders/vertexShader.glsl");
    const char* vertexShaderSource = vertexCode.c_str();
    unsigned int vertexShader;
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    std::string fragmentCode = readFile("src/shaders/fragmentShader.glsl");
    const char* fragmentShaderSource = fragmentCode.c_str();
    unsigned int fragmentShader;
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    // Create Shader Program
    shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // Screen setup

    glViewport(0, 0, screenSizeX, screenSizeY);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    // Object Context
    {
        // Particle Creation
        ParticleManager particleManager = ParticleManager(15);
        // Declare object manager
        std::vector<std::unique_ptr<Particle>>& objectArray = particleManager.objects;

        for (int i = 0; i < particles; i++)
        {
            particleManager.add((float) distx(gen), (float) disty(gen), particleRadius); // adds new particle;
        }
        
        // Shader Setup
        view = glm::mat4(1.0f);
        projection = glm::ortho(0.0f, (float) screenSizeX, 0.0f, (float) screenSizeY, -1.0f, 1.0f);

        speedLoc = glGetUniformLocation(shaderProgram, "speedSq");
        viewLoc = glGetUniformLocation(shaderProgram, "view");
        projLoc = glGetUniformLocation(shaderProgram, "projection");
        modelLocation = glGetUniformLocation(shaderProgram, "model");

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        
        // Shader Compilation Debug

        int success;
        char infoLog[512];
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            std::cout << "Vertex Shader Error:\n" << infoLog << std::endl;
        }

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            std::cout << "Fragment Shader Error:\n" << infoLog << std::endl;
        }

        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
            std::cout << "Shader Program Link Error:\n" << infoLog << std::endl;
        }

        // Delta Time
        float prevFrame = 0.0f;
        float currentFrame;
        float deltaTime;
        const float minTime = 1.0f / 120.0f;

        // SPH Setup
        area = 10.0f / (M_PI * smoothingRadius * smoothingRadius); // 2D normalization
        ghostCell = generateGhostCell();

        // Main Loop
        while(!glfwWindowShouldClose(window))
        {
            // Calculate the frame difference
            currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - prevFrame;
            if (deltaTime < minTime) {
                //std::cout << (1.0f / deltaTime) << "\n";
                float sleepTime = minTime - deltaTime;
                usleep(sleepTime * 1e6f);
            }
            currentFrame = static_cast<float>(glfwGetTime());
            deltaTime = currentFrame - prevFrame;
            prevFrame = currentFrame;

            // Check for window changes
            processInput(window);

            if (!frozen)
            {
                // Rendering
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);

                // Reset grid
                for (auto& cell : grid) cell.clear();
                generateGrid(objectArray);

                // Handle Cursor interactions
                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                {
                    double mouseX, mouseY;
                    glfwGetCursorPos(window, &mouseX, &mouseY);
                    mouseY = screenSizeY - mouseY;
                }

                // Calculate the densities
                for (int p = 0; p < objectArray.size(); p++)
                {
                    auto& object = objectArray[p];
                    auto& A = *object;
                    A.acceleration = glm::vec3(0.0f);
                    int gridX = std::clamp(static_cast<int>(A.position.x / gridSize), 0, cellsX - 1);
                    int gridY = std::clamp(static_cast<int>(A.position.y / gridSize), 0, cellsY - 1);
                    float density = 0.0f;
                    glm::vec3 acceleration = glm::vec3(0.0f);
                    float sumDist = 0.0f;
                    glm::vec3 sumR = glm::vec3(0.0f);
                    int neighborCount = 0;
                    for (int i = -1; i < 2; i++) {
                        for (int j = -1; j < 2; j++) {
                            if (gridX + i >= 0 && gridX + i < cellsX && gridY + j >= 0 && gridY + j < cellsY)
                            {
                                int key = (gridX + i) + (gridY + j) * cellsX;
                                for (auto& c : grid[key]) {
                                    auto& B = *objectArray[c];
                                    glm::vec3 normal = A.position - B.position;
                                    float distance = glm::length(normal);
                                    if (distance < smoothingRadius && distance > 0.00001f)
                                    {
                                        density += calculateDensity(distance, B.mass);
                                        acceleration += calculateGradient(distance, B.mass) * normal;
                                        sumDist += distance;
                                        sumR += normal;
                                        neighborCount++;
                                    }
                                }
                            }
                            else
                            {
                                float offsetX = (gridX + i) * gridSize;
                                float offsetY = (gridY + j) * gridSize;

                                for (auto& ghostOffset : ghostCell) {
                                    glm::vec3 normal = A.position - glm::vec3(offsetX + ghostOffset.x, offsetY + ghostOffset.y, 0.0f);
                                    float distance = glm::length(normal);
                                    if (distance < smoothingRadius && distance > 0.00001f)
                                    {
                                        density += calculateDensity(distance, A.mass);
                                        acceleration += calculateGradient(distance, A.mass) * normal;
                                        sumDist += distance;
                                        sumR += normal;
                                        neighborCount++;
                                    }
                                }
                            }
                        }
                    }
                    // Calculating Pressure
                    if (neighborCount > 0) {
                        float avgDist = sumDist / neighborCount;
                        glm::vec3 avgR = sumR / (float) neighborCount;
                        acceleration -= calculateGradient(avgDist, A.mass) * avgR;

                        // Ring Correction
                        if (glm::length(sumR) / sumDist < 0.05f) {
                            acceleration += correction * glm::vec3((distx(gen) - 395.0f) / 790.0f, (disty(gen) - 300.0f) / 600.0f, 0.0f);
                        }
                    }

                    acceleration *= (3.0f * stiffness) / std::max(density, 0.001f);
                    A.acceleration = (float) scaleCoefficent * acceleration;
                    A.density = density / area;
                }

                if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                    {
                        double mouseX, mouseY;
                        glfwGetCursorPos(window, &mouseX, &mouseY);
                        mouseY = screenSizeY - mouseY;
                        glm::vec3 mousePos = glm::vec3((float) mouseX, (float) mouseY, 0.0f);
                        int gridKey = (int) (mouseX / gridSize) + (int) (mouseY / gridSize) * cellsX;

                        for (auto& c : grid[gridKey]) {
                            auto& P = *objectArray[c];
                            glm::vec3 direction = glm::normalize(P.position - mousePos);
                            P.acceleration += scaleCoefficent * mouseForce * direction;
                        }
                    }

                // Update Accelerations
                for (int p = 0; p < objectArray.size(); p++)
                {
                    (*objectArray[p]).updatePosition(deltaTime);
                }

                // Draw
                particleManager.draw();

                // Update
                glfwSwapBuffers(window);
            }
            glfwPollEvents();
        }
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);  
    glfwTerminate();
    return 0;
}