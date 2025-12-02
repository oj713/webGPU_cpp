// Include the C++ wrapper instead of the raw header(s)
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>

// no need to add wgpu prefix in front of everything
using namespace wgpu;

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
        TextureView GetNextSurfaceTextureView();
    
    private:
        // shared vars between init and main loop
        GLFWwindow *window;
        Device device = nullptr;
        Queue queue = nullptr;
        Surface surface = nullptr; // connects device to window
        std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle; 
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
    Instance instance = wgpuCreateInstance(nullptr);
    // Check instance
    if (!instance) {
        std::cerr << "could not initialise webgpu" << std::endl;
        return 1;
    }

    // Get Adapter
    surface = glfwGetWGPUSurface(instance, window);

    std::cout << "Requesting adapter..." << std::endl;
    RequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = surface;
    Adapter adapter = instance.requestAdapter(adapterOpts);
    std::cout << "Got adapter: " << adapter << std::endl;
    // inspectAdapter(adapter);
    // No longer need instance once we have adapter -- instance not destroyed bc referenced by adapter.
    instance.release();

    // Get Device
    std::cout << "Requesting device..." << std::endl;
    DeviceDescriptor deviceDesc = {};
    // specify to minimal options (reference webgpu.h for arguments)
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
    device = adapter.requestDevice(deviceDesc);
    std::cout << "Got device: " << device << std::endl;

    // Uncaptured error callbacks happen when we misuse the API, informative feedback. SET AFTER DEVICE CREATION 
    uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	});
    // inspectDevice(device);

    // Look at Queue
    queue = device.getQueue();

    // Configure the surface
    SurfaceConfiguration config = {};
    config.width = 640;
    config.height = 480;
    // specificy params for textures in swap chain
    TextureFormat surfaceFormat = surface.getPreferredFormat(adapter);
    config.format = surfaceFormat;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.usage = TextureUsage::RenderAttachment; // dictates memory organisation
    config.device = device;
    config.presentMode = PresentMode::Fifo; // first in, first out (compared to immediate, mailbox w single queue)
    config.alphaMode = CompositeAlphaMode::Auto; // how textures are on the window, useful for transparency
    surface.configure(config);

    adapter.release();
    return true;
}

void Application::Terminate() {
    glfwDestroyWindow(window);
    glfwTerminate();

    queue.release();
    device.release();
    surface.unconfigure();
    surface.release();
}

void Application::MainLoop() {
    glfwPollEvents();

    // get next target texture view
    TextureView targetView = GetNextSurfaceTextureView();
    if (!targetView) {return;}
    
    // Create command encoder
	CommandEncoderDescriptor encoderDesc = {};
	encoderDesc.label = "My command encoder";
	CommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    // Create render pass that clears screen with color
    RenderPassDescriptor renderPassDesc = {};

    // Attachment describes the target texture of the pass
    RenderPassColorAttachment renderPassColorAttachment = {};
    renderPassColorAttachment.view = targetView; // texture view to draw in
    renderPassColorAttachment.resolveTarget = nullptr; // not relevant while there is no multi-sampling
    renderPassColorAttachment.loadOp = LoadOp::Clear; // load operation to perform prior to execution
    renderPassColorAttachment.storeOp = StoreOp::Store; // op after executing render pass (stored or discarded)
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
    RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);
    renderPass.end();
    renderPass.release();

    // Finally encode and submit the render pass
	CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

	std::cout << "Submitting command..." << std::endl;
    queue.submit(1, &command);
    command.release();
	std::cout << "Command submitted." << std::endl;

    //end of frame
    targetView.release();
    wgpuTextureViewRelease(targetView);
#ifndef __EMSCRIPTEN__
    surface.present();
#endif

#if defined(WEBGPU_BACKEND_DAWN)
    device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
    //device.poll(false);
#endif
}

bool Application::IsRunning() {
    return !glfwWindowShouldClose(window);
}

TextureView Application::GetNextSurfaceTextureView() {
    SurfaceTexture surfaceTexture;
    // surface texture is not an object, but container for multiple returns
    // .status returns succes, .suboptimal might not issues, .texture is what we actually draw
    surface.getCurrentTexture(&surfaceTexture);
    if (surfaceTexture.status != SurfaceGetCurrentTextureStatus::Success) {
        return nullptr;
    }

    // texture view may represent a sub part of the texture. This stuff is all just copy pasted for now. 
    Texture texture = surfaceTexture.texture;
    TextureViewDescriptor viewDescriptor;
    viewDescriptor.label = "Surface texture view";
    viewDescriptor.format = texture.getFormat();
    viewDescriptor.dimension = TextureViewDimension::_2D;
    viewDescriptor.baseMipLevel = 0;
    viewDescriptor.mipLevelCount = 1;
    viewDescriptor.baseArrayLayer = 0;
    viewDescriptor.arrayLayerCount = 1;
    viewDescriptor.aspect = TextureAspect::All;
    TextureView targetView = texture.createView(viewDescriptor);

#ifndef WEBGPU_BACKEND_WGPU
    wgpuTextureRelease(surfaceTexture.texture);
#endif

    return targetView;
}
