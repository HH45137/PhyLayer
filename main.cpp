#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <PhyLayer.h>

using namespace glm;

#define PI glm::pi<float>()


// ---------- utilities (shaders, mesh) ----------
static void checkShaderCompile(GLuint shader)
{
    GLint ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        GLint len;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetShaderInfoLog(shader, len, nullptr, &log[0]);
        std::cerr << "Shader compile error: " << log << std::endl;
    }
}

static void checkProgramLink(GLuint prog)
{
    GLint ok;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        GLint len;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(len, '\0');
        glGetProgramInfoLog(prog, len, nullptr, &log[0]);
        std::cerr << "Program link error: " << log << std::endl;
    }
}

static GLuint createShaderProgram(const char* vsSrc, const char* fsSrc)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsSrc, nullptr);
    glCompileShader(vs);
    checkShaderCompile(vs);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsSrc, nullptr);
    glCompileShader(fs);
    checkShaderCompile(fs);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    checkProgramLink(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

const char* vertexShaderSrc = R"glsl(
#version 330 core

layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProj;

out vec3 vNormal;
out vec3 vWorldPos;

void main(){
    vec4 world = uModel * vec4(aPos,1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;

    gl_Position = uProj * uView * world;
}
)glsl";

const char* fragmentShaderSrc = R"glsl(
#version 330 core

in vec3 vNormal;
in vec3 vWorldPos;

out vec4 FragColor;

uniform vec3 uColor;
uniform vec3 uLightPos;
uniform vec3 uViewPos;

void main(){
    vec3 N = normalize(vNormal);
    vec3 L = normalize(uLightPos - vWorldPos);

    float diff = max(dot(N,L), 0.0);

    vec3 viewDir = normalize(uViewPos - vWorldPos);
    vec3 reflectDir = reflect(-L, N);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);

    vec3 ambient = 0.08 * uColor;
    vec3 color = ambient + (0.9 * diff + 0.4 * spec) * uColor;

    FragColor = vec4(color, 1.0);
}
)glsl";

struct Mesh
{
    GLuint VAO = 0, VBO = 0, EBO = 0;
    GLsizei indexCount = 0;

    void upload(const std::vector<float>& verts, const std::vector<unsigned int>& idx)
    {
        indexCount = (GLsizei)idx.size();
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glBindVertexArray(0);
    }

    void draw() const
    {
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }

    void destroy()
    {
        if (EBO)
            glDeleteBuffers(1, &EBO);
        if (VBO)
            glDeleteBuffers(1, &VBO);
        if (VAO)
            glDeleteVertexArrays(1, &VAO);
    }
};

Mesh createCubeMesh()
{
    std::vector<float> v = {
        -0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, -0.5f, 0.5f, 0, 0, 1, 0.5f, 0.5f, 0.5f, 0, 0, 1,
        -0.5f, 0.5f, 0.5f, 0, 0, 1, -0.5f, -0.5f, -0.5f, 0, 0, -1, 0.5f, -0.5f, -0.5f, 0, 0, -1,
        0.5f, 0.5f, -0.5f, 0, 0, -1, -0.5f, 0.5f, -0.5f, 0, 0, -1, -0.5f, -0.5f, -0.5f, -1, 0, 0,
        -0.5f, -0.5f, 0.5f, -1, 0, 0, -0.5f, 0.5f, 0.5f, -1, 0, 0, -0.5f, 0.5f, -0.5f, -1, 0, 0,
        0.5f, -0.5f, -0.5f, 1, 0, 0, 0.5f, -0.5f, 0.5f, 1, 0, 0, 0.5f, 0.5f, 0.5f, 1, 0, 0,
        0.5f, 0.5f, -0.5f, 1, 0, 0, -0.5f, 0.5f, 0.5f, 0, 1, 0, 0.5f, 0.5f, 0.5f, 0, 1, 0,
        0.5f, 0.5f, -0.5f, 0, 1, 0, -0.5f, 0.5f, -0.5f, 0, 1, 0, -0.5f, -0.5f, 0.5f, 0, -1, 0,
        0.5f, -0.5f, 0.5f, 0, -1, 0, 0.5f, -0.5f, -0.5f, 0, -1, 0, -0.5f, -0.5f, -0.5f, 0, -1, 0};
    std::vector<unsigned int> idx = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 8, 9, 10, 10, 11, 8,
                                     12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};
    Mesh m;
    m.upload(v, idx);
    return m;
}

Mesh createUVSphere(int lon = 24, int lat = 16)
{
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    for (int y = 0; y <= lat; y++)
    {
        float v = (float)y / (float)lat;
        float phi = v * (float)PI;
        for (int x = 0; x <= lon; x++)
        {
            float u = (float)x / (float)lon;
            float theta = u * (float)(2.0 * PI);
            float sx = std::sin(phi) * std::cos(theta);
            float sy = std::cos(phi);
            float sz = std::sin(phi) * std::sin(theta);
            verts.push_back(sx * 0.5f);
            verts.push_back(sy * 0.5f);
            verts.push_back(sz * 0.5f);
            verts.push_back(sx);
            verts.push_back(sy);
            verts.push_back(sz);
        }
    }
    for (int y = 0; y < lat; y++)
    {
        for (int x = 0; x < lon; x++)
        {
            int a = y * (lon + 1) + x;
            int b = a + lon + 1;
            idx.push_back(a);
            idx.push_back(b);
            idx.push_back(a + 1);
            idx.push_back(a + 1);
            idx.push_back(b);
            idx.push_back(b + 1);
        }
    }
    Mesh m;
    m.upload(verts, idx);
    return m;
}

// ---------- scene / camera ----------
struct Entity
{
    int type; // 0=cube,1=sphere
    vec3 color;
    mat4 model;
    vec3 scale;

    // optional physics handles if physics system exists
    std::optional<Physics::BodyHandle> body;
    std::optional<Physics::ShapeHandle> shape;
};

std::vector<Entity> entities;
Mesh cubeMesh, sphereMesh;
Mesh groundMesh;

struct Camera
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

vec3 screenPosToWorldRay(double mouseX, double mouseY, const mat4& view, const mat4& proj)
{
    float mx = (float)mouseX;
    float my = (float)(SCR_H - mouseY);
    ivec4 vp = ivec4(0, 0, SCR_W, SCR_H);
    vec3 nearWorld = vec3(glm::unProject(vec3(mx, my, 0.0f), view, proj, vp));
    vec3 farWorld = vec3(glm::unProject(vec3(mx, my, 1.0f), view, proj, vp));
    vec3 dir = normalize(farWorld - nearWorld);
    return dir;
}

bool raycastGround(const vec3& origin, const vec3& dir, vec3& outPos)
{
    if (fabs(dir.y) < 1e-6) return false;
    float t = (0.0f - origin.y) / dir.y;
    if (t < 0) return false;
    outPos = origin + dir * t;
    return true;
}

// Physics world pointer: currently null (no backend implemented).
std::unique_ptr<Physics::IPhysicsWorld> physicsWorld = nullptr;
// If you implement a backend, set physicsWorld = CreatePhysicsWorld(Backend::ODE) (or other) at startup.

// spawn (uses physicsWorld if available)
void spawnAtWithOptionalPhysics(const vec3& worldPos)
{
    Entity e;
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
            // Adapter must interpret box params (note: no params here; adapter may need further API)
            Physics::ShapeHandle sh = physicsWorld->CreateShape(sd);
            physicsWorld->AttachShape(bh, sh);
            e.body = bh;
            e.shape = sh;
        }
        else
        {
            Physics::ShapeDesc sd;
            sd.type = Physics::ShapeType::Sphere;
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

void processMovement(float dt)
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

int main()
{
    srand((unsigned int)time(nullptr));
    if (!glfwInit())
    {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* window =
        glfwCreateWindow(SCR_W, SCR_H, "Spawn Scene w/ Physics Interface (no backend)", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window,
                               [](GLFWwindow* w, int button, int action, int mods)
                               {
                                   mouse_button_callback(w, button, action, mods);
                                   if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
                                   {
                                       double mx, my;
                                       glfwGetCursorPos(w, &mx, &my);
                                       mat4 view = lookAt(cam.pos, cam.pos + cam.front(), vec3(0, 1, 0));
                                       mat4 proj =
                                           perspective(radians(cam.fov), (float)SCR_W / (float)SCR_H, 0.1f, 100.0f);
                                       vec3 dir = screenPosToWorldRay(mx, my, view, proj);
                                       vec3 hit;
                                       if (raycastGround(cam.pos, dir, hit))
                                       {
                                           spawnAtWithOptionalPhysics(hit);
                                       }
                                   }
                               });
    glfwSetCursorPosCallback(window, cursor_pos_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "Failed to init GLAD\n";
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    GLuint program = createShaderProgram(vertexShaderSrc, fragmentShaderSrc);
    cubeMesh = createCubeMesh();
    sphereMesh = createUVSphere(36, 18);

    // ground
    {
        std::vector<float> gv;
        std::vector<unsigned int> gi;
        float s = 50.0f;
        gv = {-s, 0.0f, -s, 0, 1, 0, s, 0.0f, -s, 0, 1, 0, s, 0.0f, s, 0, 1, 0, -s, 0.0f, s, 0, 1, 0};
        gi = {0, 1, 2, 2, 3, 0};
        groundMesh.upload(gv, gi);
    }

    // Enable Box3D physics backend (requires linking box3d and include path)
    physicsWorld = Physics::CreatePhysicsWorld(Physics::Backend::Box3D);
    if (physicsWorld)
    {
        physicsWorld->SetGravity(Physics::Vec3(0.0f, -9.81f, 0.0f));
    }
    else
    {
        // CreatePhysicsWorld returned nullptr: backend not available at link time
        // Program will continue in visual-only mode.
    }

    vec3 lightPos(5.0f, 8.0f, 5.0f);
    float lastTime = (float)glfwGetTime();

    while (!glfwWindowShouldClose(window))
    {
        float cur = (float)glfwGetTime();
        float dt = cur - lastTime;
        lastTime = cur;

        processMovement(dt);
        if (physicsWorld) physicsWorld->Step(dt);

        glViewport(0, 0, SCR_W, SCR_H);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        mat4 view = lookAt(cam.pos, cam.pos + cam.front(), vec3(0, 1, 0));
        mat4 proj = perspective(radians(cam.fov), (float)SCR_W / (float)SCR_H, 0.1f, 100.0f);

        glUseProgram(program);
        GLuint locM = glGetUniformLocation(program, "uModel");
        GLuint locV = glGetUniformLocation(program, "uView");
        GLuint locP = glGetUniformLocation(program, "uProj");
        GLuint locColor = glGetUniformLocation(program, "uColor");
        GLuint locLight = glGetUniformLocation(program, "uLightPos");
        GLuint locViewPos = glGetUniformLocation(program, "uViewPos");

        glUniformMatrix4fv(locV, 1, GL_FALSE, value_ptr(view));
        glUniformMatrix4fv(locP, 1, GL_FALSE, value_ptr(proj));
        glUniform3fv(locLight, 1, value_ptr(lightPos));
        glUniform3fv(locViewPos, 1, value_ptr(cam.pos));

        // draw ground
        glUniformMatrix4fv(locM, 1, GL_FALSE, value_ptr(mat4(1.0f)));
        glUniform3f(locColor, 0.3f, 0.35f, 0.3f);
        groundMesh.draw();

        // draw entities
        for (const Entity& e : entities)
        {
            glUniformMatrix4fv(locM, 1, GL_FALSE, value_ptr(e.model));
            glUniform3fv(locColor, 1, value_ptr(e.color));
            if (e.type == 0)
                cubeMesh.draw();
            else
                sphereMesh.draw();
        }

        glfwSwapBuffers(window);
        glfwPollEvents();

        int w, h;
        glfwGetWindowSize(window, &w, &h);
        SCR_W = w;
        SCR_H = h;
    }

    cubeMesh.destroy();
    sphereMesh.destroy();
    groundMesh.destroy();
    glDeleteProgram(program);
    glfwTerminate();
    return 0;
}
