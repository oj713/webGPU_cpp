https://eliemichel.github.io/LearnWebGPU/getting-started/project-setup.html

# Commands

Create build files for project -- isolate built files from source code by playing them in *build/* directory using `-B build` option. 
Build the program and generate executable. 
Run the program. 

```
cmake . -B build
cmake --build build
build/App
```

## WebGPU

WebGPU is a *Render Hardware Interface*, meaning that it means to provide a single itnerface for multiple underlying hardwares and OSes. Within the C++ code, WebGPU is just a header file listing available procedures. However, the compiler must know where to find the implementation of these features, so we must pass these instructions explicitly. 
There are two main implementations of the native header -- [wgpu-native](https://github.com/gfx-rs/wgpu-native) and Google's [Dawn](https://dawn.googlesource.com/dawn). Here we choose to copy Dawn (more verbose) to the root folder. 