#pragma once

#include <nlohmann/json.hpp>

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
}
