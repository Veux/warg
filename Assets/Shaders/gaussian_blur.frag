uniform vec2 gauss_axis_scale;
uniform sampler2D texture0;

in vec2 frag_uv;

layout(location = 0) out vec4 RESULT;


const int kernel_width = 7;
const vec2 gauss[kernel_width] = 
{ 
	-3.0,	0.015625,
	-2.0,	0.09375,
	-1.0,	0.234375,
	0.0,	0.3125,
	1.0,	0.234375,
	2.0,	0.09375,
	3.0,	0.015625
};

void main()
{
	vec4 color = vec4(0.0);
	for(int i = 0; i < kernel_width; i++)
	{
    vec2 offset = vec2(gauss[i].x*gauss_axis_scale.x, gauss[i].x*gauss_axis_scale.y)
    vec2 sample_uv = frag_uv + offset; 
		color += texture2D(texture0, sample_uv)*gauss[i].y;
	}
	RESULT = color;
}