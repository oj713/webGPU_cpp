#include <webgpu/webgpu.h>
#include <iostream>

int main (int, char**) {
    //std::cout << "Hello, world!" << std::endl;

    // Create WebGPU instance
    // Create descriptor (specifies options for setup)
    WGPUInstanceDescriptor desc = {};
    desc.nextInChain = nullptr; // allow for future custom expansions
    // Create instance using the descriptor
    #ifdef WEBGPU_BACKEND_EMSCRIPTEN
        WGPUInstance instance = wgpuCreateInstance(nullptr);
    #else // WEBGPU_BACKEND_EMSCRIPTEN
        WGPUInstance instance = wgpuCreateInstance(&desc);
    #endif

    // Check instance
    if (!instance) {
        std::cerr << "could not initialise webgpu" << std::endl;
        return 1;
    }
    std::cout << "wgpu instance: " << instance << std::endl;

    // Release instance (lifetime management)
    wgpuInstanceRelease(instance);

    return 0;
}