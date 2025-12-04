//ResourceManager.h
#pragma once

#include <webgpu/webgpu.hpp>

#include <vector>
#include <filesystem>

class ResourceManager {
    public:
        /**
         * Loads a file from 'path' using ad-hoc format, populates input vectors.
         */
        static bool loadGeometry(
            const std::filesystem::path& path,
            std::vector<float>& pointData,
            std::vector<uint16_t>& indexData
        );

        /**
         * Create a shader module for a given WebGPU 'device' from wgsl shader source loaded from path
         */
        static wgpu::ShaderModule loadShaderModule(
            const std::filesystem::path& path,
            wgpu::Device device
        );
    private:
};