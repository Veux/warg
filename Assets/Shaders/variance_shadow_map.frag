#version 330
varying vec4 frag_position;
layout(location = 0) out vec2 out0;
void main()
{
	float depth = frag_position.z / frag_position.w;
	depth = depth * 0.5 + 0.5;

	float moment1 = depth;
	float moment2 = depth * depth;

	float dx = dFdx(depth);
	float dy = dFdy(depth);
	moment2 += 0.25*(dx*dx+dy*dy);

 
	out0 = vec2(moment1,moment2);
}