https://eliemichel.github.io/LearnWebGPU/getting-started/project-setup.html

# Commands

Create build files for project -- isolate built files from source code by playing them in *build/* directory using `-B build` option. 
Build the program and generate executable. 
Run the program. 

```
cmake . -B build
cmake -B build -DWEBGPU_BACKEND=WGPU
# or
cmake -B build -DWEBGPU_BACKEND=DAWN
cmake --build build
build/App
```

## WebGPU

WebGPU is a *Render Hardware Interface*, meaning that it means to provide a single itnerface for multiple underlying hardwares and OSes. Within the C++ code, WebGPU is just a header file listing available procedures. However, the compiler must know where to find the implementation of these features, so we must pass these instructions explicitly. 
There are two main implementations of the native header -- [wgpu-native](https://github.com/gfx-rs/wgpu-native) and Google's [Dawn](https://dawn.googlesource.com/dawn). Here we choose to copy Dawn (more verbose) to the root folder. 

## [Adapters](https://eliemichel.github.io/LearnWebGPU/getting-started/adapter-and-device/the-adapter.html#lit-1)
Must select *adapter* before choosing device. Host system may expose multiple adapters if there are multiple GPUs available or something for a virtual device. Adapters assess capabilities of user's hardware, which then determine behavior of application. Devices are created based on chosen capabilities. Not possible to inadvertently rely on device-specific capabilities. 

```{c++}
// asynchronous
void wgpuInstanceRequestAdapter(
    WGPUInstance instance, // instance
    WGPU_NULLABLE WGPURequestAdapterOptions const * options, // options
    WGPURequestAdapterCallback callback, //function to call when request ends
    void * userdata //forwarded to requestadapter to share context information
)
```

Adapters provide info about underlying implementation and hardware -- limits = regroup maxmin values, eg texture size `wgpuAdapterGetLimits`. Features = Non-mandatory extensions capability `wgpuAdapterEnumerateFeatures`. Properties = extra information like name and vendor `wgpuAdapterGetProperties`. 

## [Device](https://eliemichel.github.io/LearnWebGPU/getting-started/adapter-and-device/the-device.html)

A WebGPU device represents the context of use of the API. All objects created are owned by the device. 
A device is requested from adapter by specifying a subset of limits and features we care about. After creation of the device, we stop using the adapter.

We can use *callback functions* to get notified if the device undergoes an error or is no longer available, which is helpful for debugging. 

## [Command Queue](https://eliemichel.github.io/LearnWebGPU/getting-started/the-command-queue.html)

Key concept for WebGPU + many other graphics APIs

Two simultaneous processes -- CPU (host, content timeline), and GPU (device, queue timeline)

* Code we write runs on CPU, some of which triggers ops on the GPU. Only exceptions are *shaders* which run on GPU
* Processes are "far away", so communication takes time

Commands intended for GPU are batched and fired through "command queue" -- GPU consumes queue when its ready, minimizing idle time

`wgpuQueueSubmit`sends commands, and `wgpuQueueWriteBuffer`and `wgpuQueueWriteTexture`send data from RAM to VRAM (gpu memory). 

Commands are submit using `wgpuQueueSubmit(queue, /* number of commands */, /* pointer to command array */)`. With a single command, simply `wgpuQueueSubmit(queue, 1, &command);`. If number of commands is known at compile time, can use C array/std::array. Otherwise can use std::vector. Don't forget to release command buffers after submission.

```
// or, safer and avoid repeating the array size:
std::array<WGPUCommandBuffer, 3> commands;
commands[0] = /* [...] */;
commands[1] = /* [...] */;
commands[2] = /* [...] */;
wgpuQueueSubmit(queue, commands.size(), commands.data());

// Release:
for (auto cmd : commands) {
    wgpuCommandBufferRelease(cmd);
}
```

We can't manually create `WGPUCommandBuffer` object, must use a command encoder. Note that the basic workflow may actually finish so fast that the code finishes and the device destructs before completion! must add manual waiting time in the case of some backends (not standardized).

# Device Interactions

## Opening a window

Creating a window usually varies by platform -- using [GLFW](https://www.glfw.org/) library to unify window management APIs (common choice and ensures agnostic code). We can create a main loop, as seen below, but this will NOT work with emscripten. Instead we must create an application class.

```
glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // ignore graphics api
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // prevents resizable window
    GLFWwindow* window = glfwCreateWindow(640, 480, "Learn WebGPU", nullptr, nullptr);
    if (!window) {
        std::cerr << "Could not open window" << std::endl;
        glfwTerminate();
        return 1;
    }

    // add a loop
    while(!glfwWindowShouldClose(window)) {
        // check if user clicked on close button or triggered another event
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
```