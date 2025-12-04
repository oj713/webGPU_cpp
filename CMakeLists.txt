cmake_minimum_required(VERSION 3.0...3.25)
project(
    LearnWebGPU # name of the project, also name of vs solution if used
    VERSION 0.1.0
    LANGUAGES CXX C # programming languages used
)

if (NOT EMSCRIPTEN)
    # add 'glfw' directory, contains def of target. GLFW is window interface
    add_subdirectory(glfw)
else ()
    # create mock target that just sets link option
    add_library(glfw INTERFACE)
    target_link_options(glfw INTERFACE -sUSE_GLFW=3)
endif()
# custom function to get the surface, OS agnostic
add_subdirectory(glfw3webgpu)
# include webgpu directory to define 'webgpu' target
add_subdirectory(webgpu)
# minimal project -- create a target of type executable, called App, source code is main.cpp
add_executable(App 
    main.cpp
    # resource manager
    ResourceManager.h
    ResourceManager.cpp)
# add webgpu target as dependency of app
target_link_libraries(App PRIVATE glfw webgpu glfw3webgpu)

# add option to enable different settings when developing app than when distributing
option(DEV_MODE "Set up development helper settings" ON)
if (DEV_MODE)
    # dev mode = load resources from source tree so that when we edit resources they're correctly versioned
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
    )
else()
    # release mode -- load resources relative to executable, ensures portability
    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="./resources"
    )
endif()

# The application's binary must find wgpu.dll or libwgpu.so at runtime,
# so we automatically copy it (it's called WGPU_RUNTIME_LIB in general)
# next to the binary. Not necessary for Dawn but would be necessary for wgpu-native
target_copy_webgpu_binaries(App)

# Set some properties by calling the set_target_properties command
set_target_properties(App PROPERTIES
    CXX_STANDARD 17 # require C++ 17
    CXX_STANDARD_REQUIRED ON # config fails if C++ 17 not supported
    CXX_EXTENSIONS OFF # disable compiler specific extensions
    COMPILE_WARNING_AS_ERROR ON # ensure no warning is left ignored
)

# compile options to ensure that as many warnings as possible are generated
if (MSVC) # assessing compiler type
    target_compile_options(App PRIVATE /W4)
else()
    target_compile_options(App PRIVATE -Wall -Wextra -pedantic)
endif()

# Options specific to emscripten web compiled version
if (EMSCRIPTEN) 
    set_target_properties(App PROPERTIES SUFFIX ".html")
    target_link_options(App PRIVATE 
        -sASYNCIFY # enable emscripten_sleep
        --preload-file "${CMAKE_CURRENT_SOURCE_DIR}/resources" # ask emscripten to bundle files within app in virtual file system
    )
endif()