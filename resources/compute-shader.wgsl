// Function shares common name with entry point defined in compute pipeline. Note @compute signal. 

// add two buffer bindings as variables in storage address space, must specify access mode
@group(0) @binding(0) var<storage, read> inputBuffer: array<f32, 64>;
@group(0) @binding(1) var<storage, read_write> outputBuffer: array<f32, 64>;

// function we want to apply to GPU side buffers for the purpose of computation
fn f(x:f32) -> f32 {
	return 2.0 * x + 1.0;
}

// workgroup size(w, d, h)
// 1D data = one dimensional series of workgroups
// Note invocation ID
@compute @workgroup_size(32, 1, 1)
fn computeStuff(@builtin(global_invocation_id) id: vec3<u32>) {
    // apply the function f to the buffer element at index id.x
    outputBuffer[id.x] = f(inputBuffer[id.x]);
}
