/** 
* tokens w @ are "attrbutes", decorate following object. Eg @builtin(vertex_index) tells us that arg in_vertex_index will be populated by built in vertex_index
* @builtin(position) means it must be intpereted by rasterizer as vertex position 
*/

/* A structure with fields labeled w vertex attribute locations, input to entry point of shader */
struct VertexInput {
	@location(0) position: vec2f,
	@location(1) color: vec3f,
};

/* struct w fields labeled as builtins, locations used as output of vertex shader (thus input of fragment shader) */
struct VertexOutput {
	@builtin(position) position: vec4f,
	// The location here does not refer to a vertex attribute, it just means that this field must be handled by the rasterizer.
	@location(0) color: vec3f,
};

/** structure holding uniform values */
struct MyUniforms {
	color: vec4f,
	time: f32, 
};

// simple uniform declaration. 
// labelled var w address space (stored in uniform space)
// binding(0) is the buffer to which uTime is bound
// group defines the binding group & thus also about memory location
@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms; 

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput; 
    let ratio = 640.0 / 480.0; // width & height of target surface. Fixes incorrect ratio
	var offset = vec2f(-0.6875, -0.463); // offset
	// move scene depending on uTime
	offset += 0.3 * vec2f(cos(uMyUniforms.time), sin(uMyUniforms.time));
	out.position = vec4f(in.position.x + offset.x, (in.position.y + offset.y) * ratio, 0.0, 1.0); 
	out.color = in.color; // forward the color attribute to the fragment shader
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	let color = in.color * uMyUniforms.color.rgb; // multiple scene color by global uniform
	// applying a gamma correction to the color
	// converting input sRGB color to linear before the target surface converts back to sRGB
	let linear_color = pow(color, vec3f(2.2));
	return vec4f(linear_color, 1.0); // use the interpolated color coming from the vertex shader
}