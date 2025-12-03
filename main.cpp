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

// embed WGSL lang source of shader module -- will be moved to file down the line 
// tokens w @ are "attrbutes", decorate following object. Eg @builtin(vertex_index) tells us that arg in_vertex_index will be populated by built in vertex_index
// @builtin(position) means it must be intpereted by rasterizer as vertex position
const char* shaderSource = R"(
@vertex
fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {
	var p = vec2f(0.0, 0.0);
	if (in_vertex_index == 0u) {
		p = vec2f(-0.5, -0.5);
	} else if (in_vertex_index == 1u) {
		p = vec2f(0.5, -0.5);
	} else {
		p = vec2f(0.0, 0.5);
	}
	return vec4f(p, 0.0, 1.0);
}

@fragment
fn fs_main() -> @location(0) vec4f {
	return vec4f(0.0, 0.4, 1.0, 1.0);
}
)";


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

        // Substep of Initialize to create render pipeline
        void InitializePipeline();
    
    private:
        // shared vars between init and main loop
        GLFWwindow *window;
        Device device = nullptr;
        Queue queue = nullptr;
        Surface surface = nullptr; // connects device to window
        TextureFormat surfaceFormat = TextureFormat::Undefined;
        std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle; 
        RenderPipeline pipeline = nullptr;
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
    // Initialize library
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
    surfaceFormat = surface.getPreferredFormat(adapter);
    config.format = surfaceFormat;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.usage = TextureUsage::RenderAttachment; // dictates memory organisation
    config.device = device;
    config.presentMode = PresentMode::Fifo; // first in, first out (compared to immediate, mailbox w single queue)
    config.alphaMode = CompositeAlphaMode::Auto; // how textures are on the window, useful for transparency
    surface.configure(config);

    adapter.release();

    InitializePipeline();

    return true;
}

void Application::Terminate() {
    glfwDestroyWindow(window);
    glfwTerminate();

    pipeline.release();
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

    // Create render pass
    RenderPassEncoder renderPass = encoder.beginRenderPass(renderPassDesc);

    // set rendering pipeline
    renderPass.setPipeline(pipeline);
    // draw instance of 3 vertices shape
    renderPass.draw(3, 1, 0, 0);

    // End
    renderPass.end();
    renderPass.release();

    // Finally encode and submit the render pass
	CommandBufferDescriptor cmdBufferDescriptor = {};
	cmdBufferDescriptor.label = "Command buffer";
	CommandBuffer command = encoder.finish(cmdBufferDescriptor);
	encoder.release();

    queue.submit(1, &command);
    command.release();

    //end of frame
    targetView.release();
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

void Application::InitializePipeline() {
    ////////////// programmable stages
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
	shaderCodeDesc.code = shaderSource; // payload = code block
	ShaderModule shaderModule = device.createShaderModule(shaderDesc);

    ////////////// Static stages
    RenderPipelineDescriptor pipelineDesc;

    // Describe vertex pipeline state
    // fetch vertex attributes from buffers (eg position, mayble color)
    // hard code position of three vertices in this example, so no need
    pipelineDesc.vertex.bufferCount = 0;
    pipelineDesc.vertex.buffers = nullptr;
    //vertex shader -- combines shader module + entry point (name of function to call) + value assignments for constants
    pipelineDesc.vertex.module = shaderModule;
	pipelineDesc.vertex.entryPoint = "vs_main";
	pipelineDesc.vertex.constantCount = 0;
	pipelineDesc.vertex.constants = nullptr;

    // Describe primitive pipeline state -- configures primitive assembly & rasterization stages
    // transforms primitive (point, line, triangle) into series of fragments equalling pixels covered
    pipelineDesc.primitive.topology = PrimitiveTopology::TriangleList; //sequence of 3 vertices = triangle
    pipelineDesc.primitive.stripIndexFormat = IndexFormat::Undefined; // no specification = sequential
    pipelineDesc.primitive.frontFace = FrontFace::CCW; // when looking from front, corner vertices in counter clockwise order
    pipelineDesc.primitive.cullMode = CullMode::None; // do not hide faces pointing away from us (usually for optimization). Usually Front, but None better for developing

    // Describe fragment pipeline state
    // fragment shader invoked for each fragment, receives interpolated values & output the final color of fragment
    FragmentState fragmentState;
	fragmentState.module = shaderModule;
	fragmentState.entryPoint = "fs_main";
	fragmentState.constantCount = 0;
	fragmentState.constants = nullptr;

    // takes each fragments color and paints onto target color attachment. Must specify colors 
    BlendState blendState;
    // Blending equation can be set independently for rgb & alpha channels. rgb = a_s * rgb_s + (1-a_s) * rgb_d
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    // a = a_d = 0 * a_s + 1 * a_d
    blendState.alpha.srcFactor = BlendFactor::Zero;
    blendState.alpha.dstFactor = BlendFactor::One;
    blendState.alpha.operation = BlendOperation::Add;

    ColorTargetState colorTarget;
    colorTarget.format = surfaceFormat;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All; // could write to only some channels
    
    // one target (one output color attachment)
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;
    pipelineDesc.fragment = &fragmentState;

    // Describe stencil/depth pipeline state
    // not used for now, used to discard fragments that are behind other fragments on the same pixel
    pipelineDesc.depthStencil = nullptr;

    // Describe multi-sampling pipeline state
    //can split pixels into sub-elements called samples , fragment assoc to sample. value of pixel calculated as average of samples
    // for now keep this off
    pipelineDesc.multisample.count = 1; // 1 sample per pixel
    pipelineDesc.multisample.mask = ~0u; // all bits on
    pipelineDesc.multisample.alphaToCoverageEnabled = false; 

    // Describe pipeline layout
    // shaders might need access to input & output resources (buffers or textures)
    pipelineDesc.layout = nullptr; 

    pipeline = device.createRenderPipeline(pipelineDesc);
    shaderModule.release();
}