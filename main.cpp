#include <Common.h>

using namespace glm;
using namespace Common;

int main()
{
    // Random seed setup
    {
        srand((unsigned int)time(nullptr));
    }

    // GLFW and GLAD
    GLFWwindow* window = nullptr;
    {
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
        window =
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
                                           vec3 dir = screen_pos_to_world_ray(mx, my, view, proj);
                                           vec3 hit;
                                           if (raycast_ground(cam.pos, dir, hit))
                                           {
                                               spawn_at_with_optional_physics(hit);
                                           }
                                       }
                                   });
        glfwSetCursorPosCallback(window, cursor_pos_callback);

        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
        {
            std::cerr << "Failed to init GLAD\n";
            return -1;
        }
    }

    // OpenGL state setup
    {
        glEnable(GL_DEPTH_TEST);
    }

    // Shader prepare
    GLuint program = 0;
    {
        auto vertShaderSource = ShaderTools::load_shader_source("mvp.vert");
        auto fragShaderSource = ShaderTools::load_shader_source("phong.frag");
        program = ShaderTools::create_shader_program(vertShaderSource.c_str(), fragShaderSource.c_str());
    }

    // Mesh prepare
    {
        cubeMesh = Mesh::create_cube_mesh();
        sphereMesh = Mesh::create_uv_sphere(36, 18);
        groundMesh = Mesh::create_plan_mesh();
    }

    // Physical engine setup
    {
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

        // Static ground collider: without it the spawned dynamic bodies have nothing to
        // hit and simply fall forever. The visible ground plane sits at y = 0, so the
        // 1 unit thick slab is centred at y = -0.5.
        if (physicsWorld)
        {
            Physics::BodyDesc gd;
            gd.isDynamic = false;
            gd.position = vec3(0.0f, -0.5f, 0.0f);

            Physics::ShapeDesc gsd;
            gsd.type = Physics::ShapeType::Box;
            gsd.halfExtents = vec3(50.0f, 0.5f, 50.0f);

            Physics::BodyHandle groundBody = physicsWorld->CreateBody(gd);
            Physics::ShapeHandle groundShape = physicsWorld->CreateShape(gsd);
            physicsWorld->AttachShape(groundBody, groundShape);
        }
    }

    // Main loop
    {
        vec3 lightPos(5.0f, 8.0f, 5.0f);
        auto lastTime = (float)glfwGetTime();
        float physicsAccumulator = 0.0f;

        while (!glfwWindowShouldClose(window))
        {
            auto cur = (float)glfwGetTime();
            float dt = cur - lastTime;
            lastTime = cur;
            if (dt > 0.25f) dt = 0.25f; // clamp after stalls / window drags

            process_movement(dt);

            // Fixed timestep accumulation: Box3D expects a constant step (typically 1/60)
            // instead of a raw variable frame delta.
            if (physicsWorld)
            {
                const float fixedStep = 1.0f / 60.0f;
                const int maxStepsPerFrame = 5; // avoid the spiral of death
                physicsAccumulator += dt;
                int stepCount = 0;
                while (physicsAccumulator >= fixedStep && stepCount < maxStepsPerFrame)
                {
                    physicsWorld->Step(fixedStep);
                    physicsAccumulator -= fixedStep;
                    ++stepCount;
                }
                if (physicsAccumulator > fixedStep) physicsAccumulator = 0.0f;
            }

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
            for (entity_s& e : entities)
            {
                // Sync the render transform from the physics body. Without this the model
                // matrix would stay frozen at its spawn value and no motion would be visible.
                if (physicsWorld && e.body.has_value())
                {
                    Physics::Vec3 p;
                    Physics::Quat q;
                    physicsWorld->GetPosition(*e.body, p, q);
                    e.model = translate(mat4(1.0f), p) * mat4_cast(q) * scale(mat4(1.0f), e.scale);
                }

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
    }

    // Clean
    {
        cubeMesh.destroy();
        sphereMesh.destroy();
        groundMesh.destroy();
        glDeleteProgram(program);
        glfwTerminate();
    }

    return 0;
}
