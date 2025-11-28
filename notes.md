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

