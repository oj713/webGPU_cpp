// Include the C++ wrapper instead of the raw header(s)
#define WEBGPU_CPP_IMPLEMENTATION
#include <webgpu/webgpu.hpp>
#include "ResourceManager.h"

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif // __EMSCRIPTEN__

#include <iostream>
#include <cassert>
#include <vector>
#include <array>

// no need to add wgpu prefix in front of everything
using namespace wgpu;

// We define a function that hides implementation-specific variants of device polling:
void wgpuPollEvents([[maybe_unused]] Device device, [[maybe_unused]] bool yieldToWebBrowser) {
#if defined(WEBGPU_BACKEND_DAWN)
    device.tick();
#elif defined(WEBGPU_BACKEND_WGPU)
    device.poll(false);
#elif defined(WEBGPU_BACKEND_EMSCRIPTEN)
    if (yieldToWebBrowser) {
        emscripten_sleep(100);
    }
#endif
}

class Application {
    public:
        // Initialize everything, return success
        bool Initialize();

        // Unitialize everything
        void Terminate();

        // Perform computations
        void Compute();

    private:
        // Substeps of Initialize to create render pipeline
        void InitializePipeline();
        RequiredLimits GetRequiredLimits(Adapter adapter);
        void InitializeBuffers();
        void InitializeBindGroups();
    
    private:
        // shared vars between init and main loop
        Device device = nullptr;
        Queue queue = nullptr;
        std::unique_ptr<ErrorCallback> uncapturedErrorCallbackHandle; 
        ComputePipeline pipeline = nullptr;
        Buffer inputBuffer = nullptr;
        Buffer outputBuffer = nullptr;
        Buffer mapBuffer = nullptr;
        PipelineLayout layout = nullptr;
        BindGroupLayout bindGroupLayout = nullptr;
        BindGroup bindGroup = nullptr;
        uint32_t uniformStride; // Required offset for dynamic uniform buffers
        uint32_t bufferSize; // size of buffer
};

int main () {
    Application app;

    if (!app.Initialize()) {
        return 1;
    }
    // no need for a loop for computation operations
    app.Compute();

    app.Terminate();
    return 0;
}

bool Application::Initialize() {
    // setting buffer size, required for getRequiredLimits
    bufferSize = 64 * sizeof(float);

    // Create WebGPU instance
    Instance instance = wgpuCreateInstance(nullptr);
    // Check instance
    if (!instance) {
        std::cerr << "could not initialise webgpu" << std::endl;
        return 1;
    }

    std::cout << "Requesting adapter..." << std::endl;
    RequestAdapterOptions adapterOpts = {};
    adapterOpts.compatibleSurface = nullptr;
    Adapter adapter = instance.requestAdapter(adapterOpts);
    std::cout << "Got adapter: " << adapter << std::endl;
    // No longer need instance once we have adapter -- instance not destroyed bc referenced by adapter.
    instance.release();

    std::cout << "Requesting device..." << std::endl;
	DeviceDescriptor deviceDesc = {};
	deviceDesc.label = "My Device";
	deviceDesc.requiredFeatureCount = 0;
	deviceDesc.requiredLimits = nullptr;
	deviceDesc.defaultQueue.nextInChain = nullptr;
	deviceDesc.defaultQueue.label = "The default queue";
	deviceDesc.deviceLostCallback = [](WGPUDeviceLostReason reason, char const* message, void* /* pUserData */) {
		std::cout << "Device lost: reason " << reason;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	};
	// Before adapter.requestDevice(deviceDesc)
	RequiredLimits requiredLimits = GetRequiredLimits(adapter);
	deviceDesc.requiredLimits = &requiredLimits;
	device = adapter.requestDevice(deviceDesc);
	std::cout << "Got device: " << device << std::endl;
    adapter.release();

    // Uncaptured error callbacks happen when we misuse the API, informative feedback. SET AFTER DEVICE CREATION 
    uncapturedErrorCallbackHandle = device.setUncapturedErrorCallback([](ErrorType type, char const* message) {
		std::cout << "Uncaptured device error: type " << type;
		if (message) std::cout << " (" << message << ")";
		std::cout << std::endl;
	});

    // Look at Queue
    queue = device.getQueue();

    InitializePipeline();
    InitializeBuffers();
    InitializeBindGroups();

    return true;
}

void Application::Terminate() {
    layout.release();
    bindGroupLayout.release();
    bindGroup.release();
    inputBuffer.release();
    outputBuffer.release();
    mapBuffer.release();
    pipeline.release();
    queue.release();
    device.release();
}

void Application::Compute() {
    // Initialize a command encoder
    queue = device.getQueue();
    CommandEncoderDescriptor encoderDesc = Default;
    CommandEncoder encoder = device.createCommandEncoder(encoderDesc);

    // Create compute pass -- much simpler than render pass, since no fixed function stage means theres basically nothing to configure.
    ComputePassDescriptor computePassDesc;
    computePassDesc.timestampWrites = nullptr;
    ComputePassEncoder computePass = encoder.beginComputePass(computePassDesc);

    // Use compute pass

    computePass.setPipeline(pipeline);
    computePass.setBindGroup(0, bindGroup, 0, nullptr);
    // replaces "draw"
    // Infer number of workgroups from expected invocation calls
    uint32_t invocationCount = bufferSize/sizeof(float);
    uint32_t workgroupSize = 32;
    // Ceils invocationCount/workgroupSize
    uint32_t workgroupCount = (invocationCount + workgroupSize - 1) / workgroupSize;
    computePass.dispatchWorkingGroups(workgroupCount, 1, 1);

    computePass.end();
    computePass.release();

    // Add copy command
    encoder.copyBufferToBuffer(outputBuffer, 0, mapBuffer, 0, bufferSize);

    // Encode and submit GPU commands
    CommandBuffer commands = encoder.finish(CommandBufferDescriptor{});
    queue.submit(commands);

    // cleanup
    encoder.release();
    commands.release();

    // Print output
    bool done = false;
    auto handle = mapBuffer.mapAsync(MapMode::Read, 0, bufferSize, [&](BufferMapAsyncStatus status) {
        if (status == BufferMapAsyncStatus::Success) {
            const float* output = (const float*)mapBuffer.getConstMappedRange(0, bufferSize);
            for(int i = 0, i<input.size();i++) {
                std::cout << "input " << input[i] << " became " << output [i] << std::endl;
            }
            mapBuffer.unmap();
        }
        done = true;
    });
    while (!done) {
        wgpuPollEvents(device, true /*yieldToBrowser*/);
    }
}

void Application::InitializePipeline() {
    ////////////// programmable stages
    std::cout << "Creating compute shader module…" << std::endl;
    ShaderModule computeShaderModule = ResourceManager::loadShaderModule(RESOURCE_DIR "/compute-shader.wgsl", device);
    std::cout << "Compute Shader Module: " << computeShaderModule << std::endl;
    
    if (computeShaderModule == nullptr) {
        std::cerr << "Could not load shader" << std::endl;
        exit(1);
    }

    ComputePipelineDescriptor computePipelineDesc = Default;
    computePipelineDesc.compute.entryPoint = "computeStuff"; // computestuff defined in shader file
    computePipelineDesc.compute.module = computeShaderModule;

    // Initialize binding group layout
    std::vector<BindGroupLayoutEntry> bindings(2, Default);

    // input buffer
    bindings[0].binding = 0;
    bindings[0].buffer.type = BufferBindingType::ReadOnlyStorage;
    bindings[0].visibility = ShaderStage::Compute;

    // Output buffer
    bindings[1].binding = 1;
    bindings[1].buffer.type = BufferBindingType::Storage;
    bindings[1].visibility = ShaderStage::Compute;

    BindGroupLayoutDescriptor bindGroupLayoutDesc;
    bindGroupLayoutDesc.entryCount = (uint32_t)bindings.size();
    bindGroupLayoutDesc.entries = bindings.data();
    BindGroupLayout bindGroupLayout = device.createBindGroupLayout(bindGroupLayoutDesc);

    // Create compute pipeline layout
    PipelineLayoutDescriptor pipelineLayoutDesc;
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = (WGPUBindGroupLayout*)&bindGroupLayout;
    PipelineLayout pipelineLayout = device.createPipelineLayout(pipelineLayoutDesc);
    computePipelineDesc.layout = pipelineLayout;

    pipeline = device.createComputePipeline(computePipelineDesc);
    computeShaderModule.release();
    pipelineLayout.release();
}

RequiredLimits Application::GetRequiredLimits(Adapter adapter) {
    //get adapter supported limits in case needed
    SupportedLimits supportedLimits;
	adapter.getLimits(&supportedLimits);

    RequiredLimits requiredLimits = Default;

    requiredLimits.limits.maxBufferSize = bufferSize; // 15 = num points, 5 = attributes per point
    requiredLimits.limits.maxBindGroups = 2; 

    // Buffers with storage usage are confronted to device limits
    requiredLimits.limits.maxStorageBuffersPerShaderStage = 2;
    requiredLimits.limits.maxStorageBufferBindingSize = bufferSize; // max buffer size

    // Limits related to choice of workgroup size/count
    // w, h, d
    requiredLimits.limits.maxComputeWorkgroupSizeX = 32;
    requiredLimits.limits.maxComputeWorkgroupSizeY = 1;
    requiredLimits.limits.maxComputeWorkgroupSizeZ = 1;
    requiredLimits.limits.maxComputeInvocationsPerWorkgroup = 32; // w * h * d
    // max(x, y, z) = workgroupCount = 64/32 = 2
    requiredLimits.limits.maxComputeWorkgroupsPerDimension = 2;

    // These two limits are different because they are "minimum" limits,
    // they are the only ones we may forward from the adapter's supported
    // limits.
    requiredLimits.limits.minUniformBufferOffsetAlignment = supportedLimits.limits.minUniformBufferOffsetAlignment;
    requiredLimits.limits.minStorageBufferOffsetAlignment = supportedLimits.limits.minStorageBufferOffsetAlignment;

    return requiredLimits;
}

void Application::InitializeBuffers() {
    // Input buffer
    BufferDescriptor bufferDesc;
	bufferDesc.size = bufferSize;
	bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Storage; // Storage usage here!
	bufferDesc.mappedAtCreation = false;
	inputBuffer = device.createBuffer(bufferDesc);

    // output buffer
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::Storage;
    outputBuffer = device.createBuffer(bufferDesc);

    //map buffer to transmit data back to cpu
    bufferDesc.usage = BufferUsage::CopyDst | BufferUsage::MapRead;
    mapBuffer = device.createBuffer(bufferDesc);

    // add data to input buffer
    std::vector<float>input(bufferSize/sizeof(float));
    for (int i = 0; i < input.size(); ++i) {
        input[i] = 0.1f * i;
    }
    queue.writeBuffer(inputBuffer, 0, input.data(), bufferSize);
}

// Create a bind-group layout that matches bindings in wgsl 
void Application::InitializeBindGroups() {
    // compute bind group
    std::vector<BindGroupEntry> entries(2, Default);

    // input buffer
    entries[0].binding = 0;
    entries[0].buffer = inputBuffer;
    entries[0].offset = 0;
    entries[0].size = bufferSize;

    // output buffer
    entries[1].binding = 1;
    entries[1].buffer = outputBuffer;
    entries[1].offset = 0;
    entries[1].size = bufferSize;

    BindGroupDescriptor bindGroupDesc{};
    bindGroupDesc.layout = bindGroupLayout;
    bindGroupDesc.entryCount = (uint32_t)entries.size();
    bindGroupDesc.entries = (WGPUBindGroupEntry*)entries.data();
    bindGroup = device.createBindGroup(bindGroupDesc);
}