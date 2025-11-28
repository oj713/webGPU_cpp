#include "webgpu-utils.h"
#include <webgpu/webgpu.h>
#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__
#include <iostream>
#include <cassert>
#include <vector>

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


    // Get adapter
    std::cout << "Requesting adapter..." << std::endl;

    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    WGPUAdapter adapter = requestAdapterSync(instance, &adapterOpts);

    std::cout << "Got adapter: " << adapter << std::endl;
    inspectAdapter(adapter);
    // No longer need instance once we have adapter -- instance not destroyed bc referenced by adapter.
    wgpuInstanceRelease(instance);

    // Get Device
    std::cout << "Requesting device..." << std::endl;
    WGPUDeviceDescriptor deviceDesc = {};
    // specify to minimal options (reference webgpu.h for arguments)
    deviceDesc.nextInChain = nullptr;
    deviceDesc.label = "My device";
    deviceDesc.requiredFeatureCount = 0; // no feature required specifically
    deviceDesc.requiredLimits = 0; // no specific limit
    deviceDesc.defaultQueue.nextInChain = nullptr;
    deviceDesc.defaultQueue.label = "The default queue";
    // notifications in case of error
    deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
        std::cout << "Device lost: reason " << reason;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    };
    WGPUDevice device = requestDeviceSync(adapter, &deviceDesc);
    std::cout << "Got device: " << device << std::endl;
    wgpuAdapterRelease(adapter);
    // Uncaptured error callbacks happen when we misuse the API, informative feedback. SET AFTER DEVICE CREATION 
    auto onDeviceError = [](WGPUErrorType type, char const* message, void* /* pUserData*/) {
        std::cout << "Uncaptured device error: type " << type;
        if (message) std::cout << " (" << message << ")";
        std::cout << std::endl;
    };
    wgpuDeviceSetUncapturedErrorCallback(device, onDeviceError, nullptr);

    inspectDevice(device);

    wgpuDeviceRelease(device);

    return 0;
}