#version 330
uniform sampler2D texture0;//height_copy
uniform sampler2D texture1;//velocity_copy
uniform float time;
uniform vec2 size;
uniform float dt;
in vec2 frag_uv;

layout(location = 0) out vec4 out0;//height
layout(location = 1) out vec4 out1;//velocity


float random ( vec2 st) {
    return fract(sin(dot(st.xy,
                         vec2(12.9898,78.233)))*
        43758.5453123);
}

float in_range(float x ,float min_edge, float max_edge)
{
    return step(x,min_edge)*step(0,-(max_edge-x));
}



float resolve_biome(vec4 biome_pack)
{
    float biome = 0.f;

    //only way we could have come out with fire on any sample is if we arent underwater so it must be valid
    //so prioritize fire


    

    float fire1 = in_range(biome_pack.r,4.f,5.f);



    return biome;
}

vec4 calc_contribution2(vec4 this_pixel,vec4 other_pixel, float velocity_into_this_pixel)
{
    float water_height = this_pixel.r;
    float ground_height = this_pixel.g;
    float other_water_height = other_pixel.r;
    float other_ground_height = other_pixel.g;










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
  float fire_height = heightmap.b;


  if(water_height < ground_height)
  {//this does not conserve water:
  //maybe it works properly when the other is uncommented
    water_height = ground_height;
    velocity = max(velocity,vec4(0));
  }


    float biome = heightmap.b;


    




        //char to dirt [0,1] // very slowly fades to dirt over time
    //dry dirt to wet dirt[1,2] //fades to dry over time
    //light grass to heavy grass[2,3] //grows fairly quickly, spreads only into wet dirt > 1.5f
    //3-4 left blank for boundary
    //light fire to heavy fire[4,5] //
    //heavy fire to char [5,6]

    //dirt values control color
    //grass values control grass coverage/color
    //grass slowly grows from 1 to 2 by default
    //if a dirt pixel is adjacent to a grass pixel, dirt will slowly progress from .5 to 1 and become grass itself

    //progress to catching fire needs to be stored somehow...
    //maybe once a pixel is within fire range, it slowly rng progresses from 3 to 4    
    //grass tile will only catch fire if fire is > 3.5 on the other pixel

    //currently we are limited only to grass growing if the soil is 'very wet'
    //and as we are in the recieve seeds range, the soil gets wetter automatically and only shows grass once it passes the 2.f threshold
    
    

























    //used to mask biome iteration steps
    float is_char_to_dirt = in_range(biome,0.f,1.f);
    float is_soil = in_range(biome,1.f,2.f);
    float is_very_wet_soil = in_range(biome,1.5f,2.f);
    float is_grass = in_range(biome,2.f,3.f);
    float is_heavy_grass = in_range(biome,2.25f,3.f);
    float on_fire = in_range(biome,4.f,5.f);
    float fire_is_fading = in_range(biome,5.f,6.f);
  float low_chance_true = float(random(vec2(time))<0.001f);

  //left
  ivec2 sample_pixel = ivec2(p.x-1,p.y);
  sample_pixel = clamp(sample_pixel,ivec2(0),ivec2(size-vec2(1)));
  vec4 other_sample = texelFetch(texture0,sample_pixel,0).rgba;
  float other_water_height = other_sample.r;
  float other_ground_height = other_sample.g;
  float velocity_into_this_pixel_from_other = velocity.r;
  vec4 left = calc_contribution2(heightmap,other_sample,velocity_into_this_pixel_from_other);
  
  
  float other_biome = other_sample.b;
  float other_grass_heavy = in_range(other_biome,2.3f,3.f);
  float other_fire_heavy = in_range(other_biome,4.25f,5.f);
  float grass_seed_spread = 0.01f*is_very_wet_soil * other_grass_heavy;//adder

  biome = biome + grass_seed_spread;
    
  //fire propagation:
  float fire_want_spread =  low_chance_true * other_fire_heavy * is_heavy_grass;
  float delta_to_fire_range = 4.f - biome;
  float fire_spread = fire_want_spread * delta_to_fire_range;//adder
  
  biome = biome + fire_spread;

  is_grass = in_range(biome,2.f,3.f);
  is_heavy_grass = in_range(biome,2.25f,3.f);
  on_fire = in_range(biome,4.f,5.f);






  //right
  sample_pixel = ivec2(p.x+1,p.y);
  sample_pixel = clamp(sample_pixel,ivec2(0),ivec2(size-vec2(1)));
  other_sample = texelFetch(texture0,sample_pixel,0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.g;
  vec4 right = calc_contribution2(heightmap,other_sample,velocity_into_this_pixel_from_other);
  
  

    
  other_biome = other_sample.b;
  other_grass_heavy = in_range(other_biome,2.3f,3.f);
  other_fire_heavy = in_range(other_biome,4.25f,5.f);
  grass_seed_spread = 0.01f*is_very_wet_soil * other_grass_heavy;//adder

  biome = biome + grass_seed_spread;
    
  //fire propagation:
  fire_want_spread =  low_chance_true * other_fire_heavy * is_heavy_grass;
  delta_to_fire_range = 4.f - biome;
  fire_spread = fire_want_spread * delta_to_fire_range;//adder
  
  biome = biome + fire_spread;

  is_grass = in_range(biome,2.f,3.f);
  is_heavy_grass = in_range(biome,2.25f,3.f);
  on_fire = in_range(biome,4.f,5.f);








  //down
  sample_pixel = ivec2(p.x,p.y-1);
  sample_pixel = clamp(sample_pixel,ivec2(0),ivec2(size-vec2(1)));
  other_sample = texelFetch(texture0,sample_pixel,0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.b;
  vec4 down = calc_contribution2(heightmap,other_sample,velocity_into_this_pixel_from_other);

    other_biome = other_sample.b;
  other_grass_heavy = in_range(other_biome,2.3f,3.f);
  other_fire_heavy = in_range(other_biome,4.25f,5.f);
  grass_seed_spread = 0.01f*is_very_wet_soil * other_grass_heavy;//adder

  biome = biome + grass_seed_spread;
    
  //fire propagation:
  fire_want_spread =  low_chance_true * other_fire_heavy * is_heavy_grass;
  delta_to_fire_range = 4.f - biome;
  fire_spread = fire_want_spread * delta_to_fire_range;//adder
  
  biome = biome + fire_spread;

  is_grass = in_range(biome,2.f,3.f);
  is_heavy_grass = in_range(biome,2.25f,3.f);
  on_fire = in_range(biome,4.f,5.f);
 
  //up
  sample_pixel = ivec2(p.x,p.y+1);
  sample_pixel = clamp(sample_pixel,ivec2(0),ivec2(size-vec2(1)));
  other_sample = texelFetch(texture0,sample_pixel,0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.a;
  vec4 up = calc_contribution2(heightmap,other_sample,velocity_into_this_pixel_from_other);

    other_biome = other_sample.b;
  other_grass_heavy = in_range(other_biome,2.3f,3.f);
  other_fire_heavy = in_range(other_biome,4.25f,5.f);
  grass_seed_spread = 0.01f*is_very_wet_soil * other_grass_heavy;//adder

  biome = biome + grass_seed_spread;
    
  //fire propagation:
  fire_want_spread =  low_chance_true * other_fire_heavy * is_heavy_grass;
  delta_to_fire_range = 4.f - biome;
  fire_spread = fire_want_spread * delta_to_fire_range;//adder
  
  biome = biome + fire_spread;

  is_grass = in_range(biome,2.f,3.f);
  is_heavy_grass = in_range(biome,2.25f,3.f);
  on_fire = in_range(biome,4.f,5.f);


      //static iterations:

    float fade_char_to_dirt = 0.01f*is_char_to_dirt;//adder
    float fade_to_dry_soil = -0.01f*is_soil;//adder
    float my_grass_grow = 0.01f*is_grass + 0.001f*is_very_wet_soil;//adder
    float fire_intensify = 0.01f*on_fire;//adder
    float fire_fade = 0.01f*fire_is_fading;//adder

  
    //if the fire has faded, reset to 0 char
    float want_reset_to_char = float(biome>6.f);
    float delta_to_char = biome;
    float char_reset = want_reset_to_char*delta_to_char;//adder



      //water over ground:

    float is_underwater = float(water_height > ground_height);
    float velocity_sum = velocity.r+velocity.g+velocity.b+velocity.a;
    
    //
    float delta_to_wet_soil = 2.f-biome;
    float reset_max_wet_soil = is_underwater*is_soil*delta_to_wet_soil;//adder

    //
    float delta_to_min_grass = 2.f-biome;
    float erode_grass = is_underwater * is_grass * abs(velocity_sum) * delta_to_min_grass * -0.01f;//adder

    //
    float delta_to_dirt = 1.f-biome;    
    float erode_char_to_dirt = is_underwater * is_char_to_dirt * abs(velocity_sum) * delta_to_dirt * 0.01f;//adder

    //
    float current_fire_intensity = clamp(biome-4.f,0.f,1.f);
    float delta_to_extinguished_destination = delta_to_char + (0.25f*(1.f-current_fire_intensity));
    float extinguish_flames = is_underwater* on_fire * delta_to_extinguished_destination;//adder

    float extinguish_fading_flames = is_underwater*fire_is_fading*(-biome);//adder





    float biome_result = biome + fade_char_to_dirt + fade_to_dry_soil + my_grass_grow + fire_intensify + fire_fade + 
                          char_reset + reset_max_wet_soil + erode_grass + erode_char_to_dirt + extinguish_flames + 
                          extinguish_fading_flames;










  float delta_height_sum = (left.r + right.r + down.r + up.r);
  float updated_water_height = water_height+delta_height_sum;
  vec4 velocity_into_this_pixel_from_others = vec4(left.g, right.g, down.g, up.g);

  if(updated_water_height < ground_height)
  {
    //velocity_into_this_pixel_from_others = max(velocity_into_this_pixel_from_others,vec4(0));
  }

  //out0 = vec4(updated_water_height,ground_height,0,1);
  out0 = vec4(updated_water_height,ground_height,biome_result,1);
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