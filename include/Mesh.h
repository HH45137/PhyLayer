#pragma once

#include <glad/glad.h>

namespace Mesh
{
    struct mesh_s
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

    static mesh_s create_cube_mesh()
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
        mesh_s m;
        m.upload(v, idx);
        return m;
    }

    static mesh_s create_uv_sphere(int lon = 24, int lat = 16)
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
        mesh_s m;
        m.upload(verts, idx);
        return m;
    }

    static mesh_s create_plan_mesh()
    {
        std::vector<float> gv;
        std::vector<unsigned int> gi;
        float s = 50.0f;
        gv = {-s, 0.0f, -s, 0, 1, 0, s, 0.0f, -s, 0, 1, 0, s, 0.0f, s, 0, 1, 0, -s, 0.0f, s, 0, 1, 0};
        gi = {0, 1, 2, 2, 3, 0};
        mesh_s m;
        m.upload(gv, gi);
        return m;
    }
}
