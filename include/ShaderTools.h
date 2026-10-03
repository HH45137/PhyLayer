#pragma once

#include <nlohmann/json.hpp>
#include <glad/glad.h>

#include <iostream>
#include <memory>
#include <string>
#include <filesystem>
#include <fstream>


namespace ShaderTools
{
    static std::string load_shader_source(const std::string& name)
    {
        const std::filesystem::path SHADER_PATH = std::filesystem::current_path().parent_path() / "shader" / name;

        const std::ifstream file(SHADER_PATH, std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("cannot open shader: " + SHADER_PATH.string());
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    static void check_shader_compile(GLuint shader)
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

    static void check_program_link(GLuint prog)
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

    static GLuint create_shader_program(const char* vsSrc, const char* fsSrc)
    {
        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vsSrc, nullptr);
        glCompileShader(vs);
        check_shader_compile(vs);
        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fsSrc, nullptr);
        glCompileShader(fs);
        check_shader_compile(fs);
        GLuint prog = glCreateProgram();
        glAttachShader(prog, vs);
        glAttachShader(prog, fs);
        glLinkProgram(prog);
        check_program_link(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return prog;
    }
}
