#version 330
uniform sampler2D texture0;//height_copy
uniform sampler2D texture1;//velocity_copy
uniform float time;
uniform vec2 size;
uniform float dt;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;//height
layout(location = 1) out vec4 out1;//velocity



vec4 calc_contribution2(float ground_height, float water_height, float other_ground_height,float other_water_height ,float velocity_into_this_pixel)
{
    float water_depth = water_height - ground_height;
    float other_water_depth = other_water_height - other_ground_height;
    const float viscosity = 51.01f;
    const float gravity = -1.0;
    const float water_pillar_edge_size = 1.0f;//used for resolution scaling
    const float water_pillar_area = water_pillar_edge_size*water_pillar_edge_size;
    float highest_ground = max(other_ground_height,ground_height);

    //outward
    float mass_of_water_above_both_ground = max(water_height-highest_ground,0);
    float force_out = mass_of_water_above_both_ground;
    //inward
    float mass_of_other_above_my_ground = max(other_water_height-highest_ground,0);
    float force_in = mass_of_other_above_my_ground;

    if(other_water_height <= other_ground_height)
    {
       velocity_into_this_pixel = min(velocity_into_this_pixel,0);
    }

    float total_force_out = (force_out - force_in);
    float water_acceleration = water_pillar_area*gravity*viscosity*dt*total_force_out;


    float ground_absorbtion_threshold = 0.0001f;
    float sintime = 0.5f+0.5f*sin(time);
    float sintimerand = float(sin(gl_FragCoord.x+gl_FragCoord.y*0.1234*time)<-0.99999f);
    float ground_absorbtion_rate = 0.000011f;
    float ground_absorbtion = 0.0f;
    float surface_tension_factor = 0.000000f;
    if(abs(water_acceleration) < surface_tension_factor)
    {
     //only absorb on negative slopes or else we absorb at shores
      if(water_depth < ground_absorbtion_threshold)// && ground_height >= other_ground_height)
      {
       ground_absorbtion = ground_absorbtion_rate;
      }
     water_acceleration = 0.f;
    }

    //not energy conserving
    //need a way to limit downward acceleration by gravity, but upward unlimited while conserving energy
    // water_acceleration = max(water_acceleration,-9.8);


    float slope = water_height / max((abs(water_height - max(other_water_height,other_ground_height))),0.000001);
    float vel_dampening = 1.f - (0.01+.000*1./slope);
    
    float updated_velocity = .991f*(velocity_into_this_pixel) + water_acceleration;
    
     //updated_velocity = (velocity_into_this_pixel) + water_acceleration;
    float delta_height = dt*updated_velocity;
    return vec4(delta_height-ground_absorbtion,updated_velocity,0,0);

}

void newmain()
{
  ivec2 p = clamp(ivec2(gl_FragCoord.xy),ivec2(0),ivec2(size-vec2(1)));
  vec4 heightmap = texelFetch(texture0,p,0).rgba;
  vec4 velocity = texelFetch(texture1,p,0).rgba;


  
  float water_height = heightmap.r; 
  float ground_height = heightmap.g;


  if(water_height < ground_height)
  {//this does not conserve water:
  //maybe it works properly when the other is uncommented
    water_height = ground_height;
    velocity = max(velocity,vec4(0));
  }

  //left
  ivec2 sample_pixel = ivec2(p.x-1,p.y);
  sample_pixel = clamp(sample_pixel,ivec2(0),ivec2(size-vec2(1)));
  vec4 other_sample = texelFetch(texture0,sample_pixel,0).rgba;
  float other_water_height = other_sample.r;
  float other_ground_height = other_sample.g;
  float velocity_into_this_pixel_from_other = velocity.r;
  vec4 left = calc_contribution2(ground_height,water_height,other_ground_height,other_water_height,velocity_into_this_pixel_from_other);
  


  //right
  sample_pixel = ivec2(p.x+1,p.y);
  sample_pixel = clamp(sample_pixel,ivec2(0),ivec2(size-vec2(1)));
  other_sample = texelFetch(texture0,sample_pixel,0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.g;
  vec4 right = calc_contribution2(ground_height,water_height,other_ground_height,other_water_height,velocity_into_this_pixel_from_other);
  
  
  //down
  sample_pixel = ivec2(p.x,p.y-1);
  sample_pixel = clamp(sample_pixel,ivec2(0),ivec2(size-vec2(1)));
  other_sample = texelFetch(texture0,sample_pixel,0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.b;
  vec4 down = calc_contribution2(ground_height,water_height,other_ground_height,other_water_height,velocity_into_this_pixel_from_other);

 
  //up
  sample_pixel = ivec2(p.x,p.y+1);
  sample_pixel = clamp(sample_pixel,ivec2(0),ivec2(size-vec2(1)));
  other_sample = texelFetch(texture0,sample_pixel,0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.a;
  vec4 up = calc_contribution2(ground_height,water_height,other_ground_height,other_water_height,velocity_into_this_pixel_from_other);



  float delta_height_sum = (left.r + right.r + down.r + up.r);
  float updated_water_height = water_height+delta_height_sum;
  vec4 velocity_into_this_pixel_from_others = vec4(left.g, right.g, down.g, up.g);

  if(updated_water_height < ground_height)
  {
    //velocity_into_this_pixel_from_others = max(velocity_into_this_pixel_from_others,vec4(0));
  }

  //out0 = vec4(updated_water_height,ground_height,0,1);
  out0 = vec4(updated_water_height,ground_height,0,1);
  out1 = velocity_into_this_pixel_from_others;






}



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
    }
    float height_difference = height_other-h;
    float acceleration = dt*height_difference;
    float updated_velocity = 0.9995f*velocity_between_pixels + acceleration;
    float delta_height = dt*updated_velocity;
    return vec4(delta_height,updated_velocity,0,0);
}


//since we cant write to any pixel but our own, we want to 
//calculate contribution from adjacent pixels flowint into this one
void main()
{
  newmain();
  return;
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
  out0 = vec4(height_result,block,0,1);
  out1 = velocity_pack;
}