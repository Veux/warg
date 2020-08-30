#version 330
uniform sampler2D texture0;//height_copy
uniform sampler2D texture1;//velocity_copy
uniform float time;
uniform vec2 size;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;//height
layout(location = 1) out vec4 out1;//velocity

float dt = .0301f;

vec4 calc_contribution(float h, ivec2 samp_loc,float velocity_between_pixels)
{
    ivec2 sample_loc = clamp(samp_loc,ivec2(0),ivec2(size-vec2(1)));
    float height_other = texelFetch(texture0,sample_loc,0).r;
    
    float block = texelFetch(texture0,sample_loc,0).g;

    if(block > 0.0f)
    {
    vec2 p = gl_FragCoord.xy;
    
    ivec2 sample_loc = clamp(ivec2(p),ivec2(0),ivec2(size-vec2(1)));
      height_other = texelFetch(texture0,sample_loc,0).r;
      //return vec4(0,velocity_between_pixels,0,0);
    }


    float height_difference = height_other-h;
    float acceleration = dt*height_difference;
    float updated_velocity = 0.9995f*velocity_between_pixels + acceleration;
    float delta_height = dt*updated_velocity;
    return vec4(delta_height,updated_velocity,0,0);
    
    //return vec4(delta_height,updated_velocity,0,0);

    //h = 0
    //velocity_other = 0
    //height_other = 1
    //height_diff = 1
    //acceleration is 0.01;
    //updated velocity is 0 + 0.01
    //delta height is 0.001
   //we return vec2(0.001,0.01)

    //other pixel:
    //h = 1
    //velocity_other = 0
    //height_other = 0
    //height_diff = -1
    //acceleration is -0.01
    //updated velocity = -0.01
    //delta_height = -0.001

    //return vec2(-0.001,-0.01)


    //if h is 0 for both pixels, and velocity is 0
    //height_difference = 0;
    //acceleration = 0;
    //updated_velocity = 0
    //delta_height = 0;
    //return vec2(0,0)

    


}


//since we cant write to any pixel but our own, we want to 
//calculate contribution from adjacent pixels flowint into this one
void main()
{
  
  //one problem: velocity between pixels should be stored once, not twice
  //currently each pixel stores a velocity value for each edge between pixels
  //this means we can diverge over time as error accumulates
  //3 options:
  //we could leave it as is (maybe its fine?)
  //we could run a shader that averages the edges out
  //we could be smarter and only store them once anyway


  ivec2 p = clamp(ivec2(gl_FragCoord.xy),ivec2(0),ivec2(size-vec2(1)));
  vec4 heightmap = texelFetch(texture0,p,0).rgba;
  vec4 velocity = texelFetch(texture1,p,0).rgba;
  
  float height = heightmap.r;
  float block = heightmap.g;
  
  vec4 left = calc_contribution(height,ivec2(p.x-1,p.y),velocity.r);
  vec4 right = calc_contribution(height,ivec2(p.x+1,p.y),velocity.g);
  vec4 down = calc_contribution(height,ivec2(p.x,p.y-1),velocity.b);
  vec4 up = calc_contribution(height,ivec2(p.x,p.y+1),velocity.a);

  

  float delta_height_sum = (left.r + right.r + down.r + up.r);
  //delta_height_sum = 4.f*delta_height_sum;
  float height_result = height+delta_height_sum;
  vec4 velocity_pack = vec4(left.g, right.g, down.g, up.g);
  

  //dampens and converges to mean
  //need a better way to do this so that it works on terrain
//  height_result = mix(height_result,0.5f,0.000f);
//  velocity_pack = mix(velocity_pack,vec4(0),0.008f);



  //velocity_pack = vec4(0);
  out0 = vec4(height_result,block,0,1);
  //out0 = velocity;
  //out0 = vec4(p/2048,0,1);
  out1 = velocity_pack;
}