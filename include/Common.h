#pragma once

#include <PhyLayer.h>
#include <ShaderTools.h>
#include <Mesh.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>


#define PI glm::pi<float>()


namespace Common
{
    using namespace glm;

    struct entity_s
    {
        int type; // 0=cube,1=sphere
        vec3 color;
        mat4 model;
        vec3 scale;

        // optional physics handles if physics system exists
        std::optional<Physics::BodyHandle> body;
        std::optional<Physics::ShapeHandle> shape;
    };

    std::vector<entity_s> entities;
    Mesh::mesh_s cubeMesh, sphereMesh;
    Mesh::mesh_s groundMesh;

    static struct camera_s
    {
        vec3 pos{0.0f, 1.6f, 5.0f};
        float yaw = -90.0f, pitch = 0.0f;
        float fov = 45.0f;

        vec3 front() const
        {
            vec3 f;
            f.x = cos(radians(yaw)) * cos(radians(pitch));
            f.y = sin(radians(pitch));
            f.z = sin(radians(yaw)) * cos(radians(pitch));
            return normalize(f);
        }

        vec3 right() const { return normalize(cross(front(), vec3(0, 1, 0))); }
        vec3 up() const { return normalize(cross(right(), front())); }
    } cam;

    bool firstMouse = true;
    double lastX = 400, lastY = 300;
    bool capturingMouse = false;
    float mouseSensitivity = 0.12f;
    bool keys[1024];
    int SCR_W = 1280, SCR_H = 720;

    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, true);
        if (key >= 0 && key < 1024)
        {
            if (action == GLFW_PRESS)
                keys[key] = true;
            else if (action == GLFW_RELEASE)
                keys[key] = false;
        }
    }

    void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
    {
        if (button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            if (action == GLFW_PRESS)
            {
                capturingMouse = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                double x, y;
                glfwGetCursorPos(window, &x, &y);
                lastX = x;
                lastY = y;
            }
            else if (action == GLFW_RELEASE)
            {
                capturingMouse = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                firstMouse = true;
            }
        }
    }

    void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
    {
        if (!capturingMouse) return;
        if (firstMouse)
        {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }
        double xoffset = xpos - lastX;
        double yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;
        cam.yaw += (float)xoffset * mouseSensitivity;
        cam.pitch += (float)yoffset * mouseSensitivity;
        if (cam.pitch > 89.0f) cam.pitch = 89.0f;
        if (cam.pitch < -89.0f) cam.pitch = -89.0f;
    }

    vec3 screen_pos_to_world_ray(double mouseX, double mouseY, const mat4& view, const mat4& proj)
    {
        float mx = (float)mouseX;
        float my = (float)(SCR_H - mouseY);
        ivec4 vp = ivec4(0, 0, SCR_W, SCR_H);
        vec3 nearWorld = vec3(glm::unProject(vec3(mx, my, 0.0f), view, proj, vp));
        vec3 farWorld = vec3(glm::unProject(vec3(mx, my, 1.0f), view, proj, vp));
        vec3 dir = normalize(farWorld - nearWorld);
        return dir;
    }

    bool raycast_ground(const vec3& origin, const vec3& dir, vec3& outPos)
    {
        if (fabs(dir.y) < 1e-6) return false;
        float t = (0.0f - origin.y) / dir.y;
        if (t < 0) return false;
        outPos = origin + dir * t;
        outPos += glm::vec3(0.0f, 0.05f, 0.0f); // tiny lift so the body does not spawn inside the ground
        return true;
    }

    // Physics world pointer: currently null (no backend implemented).
    std::unique_ptr<Physics::IPhysicsWorld> physicsWorld = nullptr;
    // If you implement a backend, set physicsWorld = CreatePhysicsWorld(Backend::ODE) (or other) at startup.

    // spawn (uses physicsWorld if available)
    void spawn_at_with_optional_physics(const vec3& worldPos)
    {
        entity_s e;
        e.type = (rand() % 2);
        e.color = vec3(0.3f + (rand() % 100) / 100.0f * 0.7f, 0.3f + (rand() % 100) / 100.0f * 0.7f,
                       0.3f + (rand() % 100) / 100.0f * 0.7f);
        float s = 0.3f + (rand() % 100) / 100.0f * 1.2f;
        e.scale = vec3(s);
        e.model = translate(mat4(1.0f), worldPos + vec3(0.0f, s * 0.5f, 0.0f));
        e.model = e.model * scale(mat4(1.0f), e.scale);

        if (physicsWorld)
        {
            // Use the physics interface to create a body and shape.
            Physics::BodyDesc bd;
            bd.isDynamic = true;
            bd.mass = 1.0f;
            bd.position = worldPos + vec3(0.0f, s * 0.5f, 0.0f);
            bd.rotation = Physics::Quat(1.0f, 0, 0, 0);

            Physics::BodyHandle bh = physicsWorld->CreateBody(bd);
            if (e.type == 0)
            {
                Physics::ShapeDesc sd;
                sd.type = Physics::ShapeType::Box;
                // The cube mesh is a unit cube (+/-0.5), so the world half extent is scale * 0.5.
                sd.halfExtents = e.scale * 0.5f;
                Physics::ShapeHandle sh = physicsWorld->CreateShape(sd);
                physicsWorld->AttachShape(bh, sh);
                e.body = bh;
                e.shape = sh;
            }
            else
            {
                Physics::ShapeDesc sd;
                sd.type = Physics::ShapeType::Sphere;
                // The sphere mesh radius is 0.5, so the world radius is scale * 0.5.
                sd.radius = e.scale.x * 0.5f;
                Physics::ShapeHandle sh = physicsWorld->CreateShape(sd);
                physicsWorld->AttachShape(bh, sh);
                e.body = bh;
                e.shape = sh;
            }
        }
        else
        {
            // No physics backend: visual-only entity
        }
        entities.push_back(e);
    }

    void process_movement(float dt)
    {
        float speed = 5.0f;
        if (keys[GLFW_KEY_LEFT_SHIFT]) speed *= 2.0f;
        vec3 f = cam.front();
        vec3 r = cam.right();
        if (keys[GLFW_KEY_W]) cam.pos += f * speed * dt;
        if (keys[GLFW_KEY_S]) cam.pos -= f * speed * dt;
        if (keys[GLFW_KEY_A]) cam.pos -= r * speed * dt;
        if (keys[GLFW_KEY_D]) cam.pos += r * speed * dt;
    }
}
