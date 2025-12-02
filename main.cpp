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
        // retrieves next target texture view
        std::pair<WGPUSurfaceTexture, WGPUTextureView> GetNextSurfaceViewData();

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

    // Configure the surface
    WGPUSurfaceConfiguration config = {};
    config.width = 640;
    config.height = 480;
    config.nextInChain = nullptr;
    // specificy params for textures in swap chain
    WGPUTextureFormat surfaceFormat = wgpuSurfaceGetPreferredFormat(surface, adapter);
    config.format = surfaceFormat;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.usage = WGPUTextureUsage_RenderAttachment; // dictates memory organisation
    config.device = device;
    config.presentMode = WGPUPresentMode_Fifo; // first in, first out (compared to immediate, mailbox w single queue)
    config.alphaMode = WGPUCompositeAlphaMode_Auto; // how textures are on the window, useful for transparency
    wgpuSurfaceConfigure(surface, &config);
   
    wgpuAdapterRelease(adapter);
    return true;
}

void Application::Terminate() {
    glfwDestroyWindow(window);
    glfwTerminate();

    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);
}

void Application::MainLoop() {
    glfwPollEvents();

    // get next target texture view
    auto [surfaceTexture, targetView] = GetNextSurfaceViewData();
    if (!targetView) {return;}
    
    // Create command encoder
	WGPUCommandEncoderDescriptor encoderDesc = {};
	encoderDesc.nextInChain = nullptr;
	encoderDesc.label = "My command encoder";
	WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    // Create render pass that clears screen with color
    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.nextInChain = nullptr;

    // Attachment describes the target texture of the pass
    WGPURenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView; // texture view to draw in
    renderPassColorAttachment.resolveTarget = nullptr; // not relevant while there is no multi-sampling
    renderPassColorAttachment.loadOp = WGPULoadOp_Clear; // load operation to perform prior to execution
    renderPassColorAttachment.storeOp = WGPUStoreOp_Store; // op after executing render pass (stored or discarded)
    renderPassColorAttachment.clearValue = WGPUColor{ 0.9, 0.1, 0.2, 1.0 }; // value to clear screen with
#ifndef WEBGPU_BACKEND_WGPU
    // not using depth buffer
    renderPassColorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
#endif
    // special attachments to come back to later
    renderPassDesc.depthStencilAttachment = nullptr;
    renderPassDesc.timestampWrites = nullptr;
    // attaching the texture to which we edit
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &renderPassColorAttachment;

    // Create render pass, end it immediately (clear screen, no draw)
    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    // finish encoding and submit
    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    // Finally encode and submit the render pass
	WGPUCommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.nextInChain = nullptr;
	cmdBufferDescriptor.label = "Command buffer";
	WGPUCommandBuffer command = wgpuCommandEncoderFinish(encoder, &cmdBufferDescriptor);
	wgpuCommandEncoderRelease(encoder);

	std::cout << "Submitting command..." << std::endl;
	wgpuQueueSubmit(queue, 1, &command);
	wgpuCommandBufferRelease(command);
	std::cout << "Command submitted." << std::endl;

    //end of frame
    wgpuTextureViewRelease(targetView);
#ifndef __EMSCRIPTEN__
    wgpuSurfacePresent(surface);
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    wgpuDeviceTick(device);
#elif defined(WEBGPU_BACKEND_WGPU)
    wgpuDevicePoll(device, false, nullptr);
#endif
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
}

std::pair<WGPUSurfaceTexture, WGPUTextureView> Application::GetNextSurfaceViewData() {
    WGPUSurfaceTexture surfaceTexture;
    // surface texture is not an object, but container for multiple returns
    // .status returns succes, .suboptimal might not issues, .texture is what we actually draw
    wgpuSurfaceGetCurrentTexture(surface, &surfaceTexture);
    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success) {
        return {surfaceTexture, nullptr};
    }

    // texture view may represent a sub part of the texture. This stuff is all just copy pasted for now. 
    WGPUTextureViewDescriptor viewDescriptor;
    viewDescriptor.nextInChain = nullptr;
    viewDescriptor.label = "Surface texture view";
    viewDescriptor.format = wgpuTextureGetFormat(surfaceTexture.texture);
    viewDescriptor.dimension = WGPUTextureViewDimension_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = WGPUTextureAspect_All;
    WGPUTextureView targetView = wgpuTextureCreateView(surfaceTexture.texture, &viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
    wgpuTextureRelease(surfaceTexture.texture);
#endif

    return {surfaceTexture, targetView};
}
