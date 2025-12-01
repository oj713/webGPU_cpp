#include "webgpu-utils.h"
#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>
#ifdef WEBGPU_BACKEND_WGPU
#   include <webgpu/wgpu.h>
#endif
#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__
#include <iostream>
#include <cassert>
#include <vector>

class Application {
    public:
        // Initialize everything, return success
        bool Initialize();

        // Unitialize everything
        void Terminate();
        
        // Draw a frame and handle events
        void MainLoop();

        // Reeturn true while loop should keep running
        bool IsRunning();

    private:
        // shared vars between init and main loop
        GLFWwindow *window;
        WGPUDevice device;
        WGPUQueue queue;
        WGPUSurface surface; // connects device to window
};

int main () {
    Application app;

    if (!app.Initialize()) {
        return 1;
    }
    
    #ifdef __EMSCRIPTEN__
        auto callback = [](void *arg) {
            //                  ^^^ we get address of app in callback
            Application* pApp = reinterpret_cast<Application*>(arg);
            //                    ^^^^^^^^^^^^ force address to be intepreted as pointer to application object
            pApp->MainLoop();
        };
        emscripten_set_main_loop_arg(callback, &app, 0, true); // pass application address
    #else   
        while (app.IsRunning()) {
            app.MainLoop();
        }
    #endif

    app.Terminate();
    return 0;
}

bool Application::Initialize() {
    // Open Window
    glfwInit(); // Initialize library
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        return 1;
    }
    // Setting extra arguments before creating window
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // ignore graphics api
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // prevents resizable window
    window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
    if (!window) {
        std::cerr << "Could not open window" << std::endl;
        glfwTerminate();
        return 1;
    }

    // Create WebGPU instance
    WGPUInstanceDescriptor desc = {}; // Create descriptor (specifies options for setup)
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


    // Get Adapter
    std::cout << "Requesting adapter..." << std::endl;
    WGPURequestAdapterOptions adapterOpts = {};
    adapterOpts.nextInChain = nullptr;
    surface = glfwGetWGPUSurface(instance, window);
    adapterOpts.compatibleSurface = surface;
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
    device = requestDeviceSync(adapter, &deviceDesc);
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

    // Look at Queue
    queue = wgpuDeviceGetQueue(device);
    return true;
}

void Application::Terminate() {
    glfwDestroyWindow(window);
    glfwTerminate();

    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuSurfaceRelease(surface);
}

void Application::MainLoop() {
    glfwPollEvents();

    #if defined(WEBGPU_BACKEND_DAWN)
	    wgpuDeviceTick(device);
    #elif defined(WEBGPU_BACKEND_WGPU)
        wgpuDevicePoll(device, false, nullptr);
    #endif
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
}
