// ResourceManager.cpp
#include "ResourceManager.h"

#include <fstream>
#include <sstream>
#include <string>
#include <filesystem>

using namespace wgpu;

bool ResourceManager::loadGeometry(
    const std::filesystem::path& path,
    std::vector<float>& pointData,
    std::vector<uint16_t>& indexData
) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }

    pointData.clear();
    indexData.clear();

    enum class Section {
        None,
        Points,
        Indices,
    };
    Section currentSection = Section::None;

    float value;
    uint16_t index;
    std::string line;
    while (!file.eof()) {
        getline(file, line);
        
        // overcome the `CRLF` problem
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        if (line == "[points]") {
            currentSection = Section::Points;
        }
        else if (line == "[indices]") {
            currentSection = Section::Indices;
        }
        else if (line[0] == '#' || line.empty()) {
            // Do nothing, this is a comment
        }
        else if (currentSection == Section::Points) {
            std::istringstream iss(line);
            // Get x, y, r, g, b
            for (int i = 0; i < 5; ++i) {
                iss >> value;
                pointData.push_back(value);
            }
        }
        else if (currentSection == Section::Indices) {
            std::istringstream iss(line);
            // Get corners #0 #1 and #2
            for (int i = 0; i < 3; ++i) {
                iss >> index;
                indexData.push_back(index);
            }
        }
    }
    return true;
}

ShaderModule loadShaderModule(
    const std::filesystem::path& path,
    Device device
) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return nullptr;
    }
    file.seekg(0, std::ios::end);
	size_t size = file.tellg();
	std::string shaderSource(size, ' ');
	file.seekg(0);
	file.read(&shaderSource[0], size);

    // shader module talks to binary language of CPU rather than GPU -- app distributed w source code of shaders & compiled on the fly
    // Shader language is "WGSL" 
    ShaderModuleDescriptor shaderDesc;
#ifdef WEBGPU_BACKEND_WGPU
	shaderDesc.hintCount = 0;
	shaderDesc.hints = nullptr;
#endif
    // NOT leaving nextInChain empty! entry point of extensions mechanism -- either null or pointing to WGPUChainedStruct
    // structed may recursively have a next element, it has a type
    ShaderModuleWGSLDescriptor shaderCodeDesc;
	// Set the chained struct's header
	shaderCodeDesc.chain.next = nullptr;
	shaderCodeDesc.chain.sType = SType::ShaderModuleWGSLDescriptor;
	// Connect the chain
	shaderDesc.nextInChain = &shaderCodeDesc.chain;
	shaderCodeDesc.code = shaderSource.c_str(); // payload = code block
	return device.createShaderModule(shaderDesc);
}