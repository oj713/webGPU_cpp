https://eliemichel.github.io/LearnWebGPU/getting-started/project-setup.html

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

Command queue only goes in one way -- CPU host to GPU device. "fire and forget" queue. In order to read back, we have to use an **async op**, set up a callback that gets invoked when data is ready

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

## [Drawing](https://eliemichel.github.io/LearnWebGPU/getting-started/first-color.html)

Render pipeline does not draw directly on the currently displayed texture. We draw to an off-screen texture, which replaces the current only when complete. There may be a queue of offscreen textures waiting to be presented. 

Thus the surface must be configured before use. 

### [Render Pipeline](https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/hello-triangle.html)

WebGPU has a render pipeline which is predefined but can be configured using a *render pipeline* object. Pipeline executes "stages", many fixed function (limited customizability) but others are programmable. In programmable stages a shader is executed across vertices/fragments

## [Buffers](https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/input-geometry/a-first-vertex-attribute.html)

buffers are just a chunk of memory allocated in the VRAM (gpu memory). `new` or `malloc` for the gpu

In order to pass data to the WGSL to allow for dynamically entered vertices, we create a buffer to store the value of the input for each vertex (GPU side); tell render pipeline how to interpret raw data (layout): set the vertex buffer in the render pass before draw call

## [Index Buffer](https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/input-geometry/index-buffer.html)

Separates list of vertex attributes from actual order they're connected. Consider that in a shape with multiple planes, listing every shape's points results in duplicated data (shared points). Usually more compact to separate position from connectivity and can thus save a lot of VRAM.

## [Uniforms](https://eliemichel.github.io/LearnWebGPU/basic-3d-rendering/shader-uniforms/a-first-uniform.html)

Uniforms are global variables inside shaders, value loaded from GPU buffer. *bound* to buffer. Value *uniform* across diff vertices/fragment of a draw call, but can change from one call to another by updating values of bound buffer

- declare the uniform in the shader
- create bound buffer
- configure properties of binding (layout)
- create a bind group (contains actual bindings, mirros layout described in pipeline and actually connects it to resources. Allows pipeline to be reused as is depending on variants of resource)

There are multiple ways to add a second uniform --
- in a different bind group
    - this option is interesting if you want to call the render pipeline w multiple uniform combinations. e.g. use a diff group to store the camera + lighting information (changes only between frames) and to store object information (location, orientation, etc, different for each draw call w/in same frame)

- in the same bind group, but a different binding
    - interest is to set different visibility depending on the binding. E.g. time is only used in vertex shader, while color only needed by fragment shader. For flexibility, option 1 makes more sense.
- in the same binding, by replacing the type of uniform w a custom struct

The architecture of the GPU imposes some constraints on organization of fields in a uniform buffer. Offset of a field of type `vec4f`must be a multiple of the size of `vec4f`(ie 16 bytes). Thus the field is **aligned** to 16 bytes. In addition they must be host-sharable, which implies a constraint on structure size. Total size must be a multiple of the alignment size of its largest field (here, multiple of 16 bytes the size of vec4f). Thus we must add padding to our structure

**Dynamic uniform buffers** allow us to issue multiple calls to the draw method (for example, two draw to objects w diff uniform values). 
* Have buffer that is twice the size of MyUniforms. First draw call - set offset to 0 to use first set of values, 2nd draw call uses offset of size(MyUniforms). Value of offset is constrained to be a multiple of the minUniformBufferOffsetAlignment limit of device (stride must be rounded up)