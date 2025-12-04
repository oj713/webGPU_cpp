/** tokens w @ are "attrbutes", decorate following object. Eg @builtin(vertex_index) tells us that arg in_vertex_index will be populated by built in vertex_index
@builtin(position) means it must be intpereted by rasterizer as vertex position */

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

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    let ratio = 640.00 / 480.00; // width & height of target surface. Fixes incorrect ratio
	var out: VertexOutput; 
	out.position = vec4f(in.position.x, in.position.y * ratio, 0.0, 1.0); 
	out.color = in.color; // forward the color attribute to the fragment shader
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	return vec4f(in.color, 1.0); // use the interpolated color coming from the vertex shader
}