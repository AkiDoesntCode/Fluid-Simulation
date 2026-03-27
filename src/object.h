#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <math.h>
#include <iostream>

//external
extern int screenSizeX;
extern int screenSizeY;
extern float restitution;
extern float gravity;

class Object {
public:
    glm::vec3 position, velocity, acceleration;
    float rotation, scaleX, scaleY, mass;
    Object(float x, float y, float _mass = 1.0f)
        : position(x, y, 0.0f),
          velocity(0.0f),
          acceleration(0.0f),
          mass(_mass),
          rotation(0.0f),
          scaleX(1.0f),
          scaleY(1.0f)
    {}

    Object(const Object&) = delete;
    Object& operator=(const Object&) = delete;

    virtual void updatePosition(float deltaTime) = 0;
    virtual ~Object() = default;
};

template <typename T>
class ObjectManager {
public:
    std::vector<std::unique_ptr<T>> objects;
    unsigned int VAO, objectVBO, managerVBO;
    ObjectManager() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &objectVBO);
        glGenBuffers(1, &managerVBO);
    }
    virtual ~ObjectManager() = default;
};

class Particle : public Object {
public:
    float radius, density;
    Particle(float x, float y, float _radius, float _mass = 1.0f)
        : Object(x, y, _mass),
          radius(_radius)
    {
        density = 0.0f;
        acceleration = glm::vec3(0.0f);
        scaleX *= radius;
        scaleY *= radius;
    }

    Particle(const Particle&) = delete;
    Particle& operator=(const Particle&) = delete;

    void updatePosition(float deltaTime) override {
        velocity += acceleration * deltaTime;
        position += velocity * deltaTime + 0.5f * acceleration * deltaTime * deltaTime;
        boundaryCollisions();
    }

    void boundaryCollisions() {
        if (position.x + radius >= (float)screenSizeX) {
            position.x = (float)screenSizeX - radius;
            velocity.x *= -restitution;
        }
        if (position.x - radius <= 0.0f) {
            position.x = radius;
            velocity.x *= -restitution;
        }
        if (position.y + radius >= (float)screenSizeY) {
            position.y = (float)screenSizeY - radius;
            velocity.y *= -restitution;
        }
        if (position.y - radius <= 0.0f) {
            position.y = radius;
            velocity.y *= -restitution;
        }
    }
};

class ParticleManager : public ObjectManager<Particle> {
private:
    struct Data {glm::vec2 position, scale; float rotation; float speed; float density;};
    std::vector<Data> objectData;
public:
    int resolution;
    ParticleManager(int _resolution) : resolution(_resolution)
    {
        // Verticies Setup
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, objectVBO);
        glBufferData(GL_ARRAY_BUFFER, generateVertices(_resolution).size() * sizeof(float), generateVertices(_resolution).data(), GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // managerVBO setup
        glBindBuffer(GL_ARRAY_BUFFER, managerVBO);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Data), (void*)offsetof(Data, position));
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Data), (void*)offsetof(Data, scale));
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Data), (void*)offsetof(Data, rotation));
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(Data), (void*)offsetof(Data, speed));
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, sizeof(Data), (void*)offsetof(Data, density));
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glEnableVertexAttribArray(3);
        glEnableVertexAttribArray(4);
        glEnableVertexAttribArray(5);
        glVertexAttribDivisor(1, 1);
        glVertexAttribDivisor(2, 1);
        glVertexAttribDivisor(3, 1);
        glVertexAttribDivisor(4, 1);
        glVertexAttribDivisor(5, 1);

        glBindVertexArray(0); //unbind
    }

    void add(float x, float y, float radius)
    {
        objects.push_back(std::make_unique<Particle>(x, y, radius));
        // Resize buffers
        objectData.resize(objects.size());
        glBindBuffer(GL_ARRAY_BUFFER, managerVBO);
        glBufferData(GL_ARRAY_BUFFER, objects.size() * sizeof(Data), nullptr, GL_DYNAMIC_DRAW);
    }

    void updateBuffer() {
        glBindBuffer(GL_ARRAY_BUFFER, managerVBO);
        for (int i = 0; i < objects.size(); i++) {
            objectData[i] = Data{
                glm::vec2(objects[i]->position),
                glm::vec2(objects[i]->scaleX, objects[i]->scaleY),
                objects[i]->rotation,
                glm::dot(objects[i]->velocity, objects[i]->velocity),
                objects[i]->density
            };
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, objects.size() * sizeof(Data), objectData.data());
    }

    void draw()
    {
        updateBuffer();
        glBindVertexArray(VAO);
        glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, resolution + 2, objects.size());
        glBindVertexArray(0); //unbind
    }

    std::vector<float> generateVertices(int resolution)
    {
        std::vector<float> verts;
        verts.push_back(0.0f);
        verts.push_back(0.0f);
        for (int i = 0; i <= resolution; i++) {
            float angle = i * 2.0f * M_PI / resolution;
            verts.push_back(std::cos(angle));
            verts.push_back(std::sin(angle));
        }
        return verts;
    }
};