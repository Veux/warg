#version 330
uniform sampler2D texture0; // height_copy
uniform sampler2D texture1; // velocity_copy
uniform float time;
uniform vec2 size;
uniform float dt;
in vec2 frag_uv;

layout(location = 0) out vec4 out0; // height
layout(location = 1) out vec4 out1; // velocity

float random(vec2 st)
{
  return fract(sin(dot(st.xy, vec2(12.9898, 78.233))) * 43758.5453123);
}

float noise(vec2 st)
{
  vec2 i = floor(st);
  vec2 f = fract(st);

  // Four corners in 2D of a tile
  float a = random(i);
  float b = random(i + vec2(1.0, 0.0));
  float c = random(i + vec2(0.0, 1.0));
  float d = random(i + vec2(1.0, 1.0));

  vec2 u = f * f * (3.0 - 2.0 * f);

  return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float in_range(float x, float min_edge, float max_edge)
{
  return float((x >= min_edge) && (x <= max_edge));
}

bool is_between(float x, float min_edge, float max_edge)
{
  return (x >= min_edge) && (x <= max_edge);
}

vec4 calc_contribution2(float ground_height, float water_height, float other_ground_height, float other_water_height,
    float velocity_into_this_pixel)
// vec4 calc_contribution2(vec4 this_pixel,vec4 other_pixel, float velocity_into_this_pixel)
{
  // float water_height = this_pixel.r;
  // float ground_height = this_pixel.g;
  // float other_water_height = other_pixel.r;
  // float other_ground_height = other_pixel.g;

  float water_depth = water_height - ground_height;
  float other_water_depth = other_water_height - other_ground_height;
  const float viscosity = 55.01f;
  const float gravity = -1.0;
  const float water_pillar_edge_size = 1.0f; // used for resolution scaling
  const float water_pillar_area = water_pillar_edge_size * water_pillar_edge_size;
  float highest_ground = max(other_ground_height, ground_height);

  // outward
  float mass_of_water_above_both_ground = max(water_height - highest_ground, 0);
  float force_out = mass_of_water_above_both_ground;
  // inward
  float mass_of_other_above_my_ground = max(other_water_height - highest_ground, 0);
  float force_in = mass_of_other_above_my_ground;

  if (other_water_height <= other_ground_height)
  {
    velocity_into_this_pixel = min(velocity_into_this_pixel, 0);
  }

  float total_force_out = (force_out - force_in);
  float water_acceleration = water_pillar_area * gravity * viscosity * dt * total_force_out;

  float ground_absorbtion_threshold = 0.0001f;
  float sintime = 0.5f + 0.5f * sin(time);
  float sintimerand = float(sin(gl_FragCoord.x + gl_FragCoord.y * 0.1234 * time) < -0.99999f);
  float ground_absorbtion_rate = 0.000011f;
  float ground_absorbtion = 0.0f;
  float surface_tension_factor = 0.000000f;
  if (abs(water_acceleration) < surface_tension_factor)
  {
    // only absorb on negative slopes or else we absorb at shores
    if (water_depth < ground_absorbtion_threshold) // && ground_height >= other_ground_height)
    {
      ground_absorbtion = ground_absorbtion_rate;
    }
    water_acceleration = 0.f;
  }

  // not energy conserving
  // need a way to limit downward acceleration by gravity, but upward unlimited while conserving energy
  // water_acceleration = max(water_acceleration,-9.8);

  float slope = water_height / max((abs(water_height - max(other_water_height, other_ground_height))), 0.000001);
  float vel_dampening = 1.f - (0.01 + .000 * 1. / slope);

  float updated_velocity = .991f * (velocity_into_this_pixel) + water_acceleration;

  // updated_velocity = (velocity_into_this_pixel) + water_acceleration;
  float delta_height = dt * updated_velocity;

  return vec4(delta_height - ground_absorbtion, updated_velocity, 0, 0);
}

void main()
{
  float debug = 1;
  ivec2 p = clamp(ivec2(gl_FragCoord.xy), ivec2(0), ivec2(size - vec2(1)));
  vec4 heightmap = texelFetch(texture0, p, 0).rgba;
  vec4 velocity = texelFetch(texture1, p, 0).rgba;

  float water_height = heightmap.r;
  float ground_height = heightmap.g;

  if (water_height < ground_height)
  {
    water_height = ground_height;
    velocity = max(velocity, vec4(0));
  }

  float sqrtdt = sqrt(dt);
  float cubertdt = pow(dt, 0.333);
  float fourthrootdt = pow(dt, .25);
  float sintime = sin(fract(time) * 2.f * 3.14159);

  float vvlow_chance_true = float(random(vec2(p) + vec2(sin(time))) < fourthrootdt * 0.001f &&
                                  random(vec2(p) + vec2(cos(2.1f * time))) < fourthrootdt * 0.001f &&
                                  random(vec2(p) + vec2(cos(1.11f * time))) < fourthrootdt * 0.1f &&
                                  random(vec2(p) + vec2(cos(1.31f * time))) < fourthrootdt * 0.1111f);
  float vlow_chance_true = float(random(vec2(p) + vec2(sin(time))) < cubertdt * 0.12f &&
                                 random(vec2(p) + vec2(cos(2.1f * time))) < cubertdt * 0.04f &&
                                 random(vec2(p) + vec2(cos(1.1f * time))) < cubertdt * 0.2f);
  float low_chance_true = float(random(vec2(p) + vec2(sin(time))) < cubertdt * 0.23f &&
                                random(vec2(p) + vec2(cos(2.1f * time))) < cubertdt * 0.08f &&
                                random(vec2(p) + vec2(cos(1.1f * time))) < cubertdt * 0.3f);
  float lowmed_chance_true = float(random(vec2(p) + vec2(sin(time))) < cubertdt * 0.34f &&
                                   random(vec2(p) + vec2(cos(2.1f * time))) < cubertdt * 0.16f &&
                                   random(vec2(p) + vec2(cos(1.1f * time))) < cubertdt * 0.4f);
  float med_chance_true = float(random(vec2(p) + vec2(sin(time))) < cubertdt * 0.65f &&
                                random(vec2(p) + vec2(cos(2.1f * time))) < cubertdt * 0.52f &&
                                random(vec2(p) + vec2(cos(1.1f * time))) < cubertdt * 0.5f);
  float high_chance_true = float(random(vec2(p) + vec2(sin(time))) < dt * 1.5f);

  float biome = heightmap.b;

  debug = 1;
  // high brush starts a fire
  if (biome > 7.f)
  {
    biome = 4.5f;
  }
  // lightning starts a fire
  if (bool(vvlow_chance_true))
  {
     biome = 4.5f;
  }
  // rain on water
  if (lowmed_chance_true != 0)
  {

    if (water_height > ground_height)
      water_height = water_height + .100435f; // raindrops
  }

  // fewer rain on ground because it kills fire too fast
  if (vlow_chance_true != 0)
  {
    if (water_height < ground_height)
      water_height = water_height + .001131f; // raindrops
  }

  float is_char_to_dirt = in_range(biome, 0.f, 1.f);
  float is_soil = in_range(biome, 1.f, 2.f);
  float is_very_wet_soil = in_range(biome, 1.5f, 2.f);
  float is_grass = in_range(biome, 2.f, 3.f);
  float on_fire = in_range(biome, 4.f, 5.f);

  // turn soil into grass:

  // spreading to adjacent tiles:
  float grass_seed_spread_chance = med_chance_true;
  float grass_seed_spread_speed = .25f;
  float grass_seed_char_to_grass = lowmed_chance_true;

  // spawns grass:
  float char_grass_spawn_chance = vvlow_chance_true; // vvlow_chance_true;
  float soil_grass_spawn_chance = vlow_chance_true;  // vlow_chance_true;
  float wet_soil_grass_spawn_chance = lowmed_chance_true;

  float grass_soil_spawn_grow_rate = 0.1f;
  float grass_very_wet_soil_spawn_grow_rate = 0.1f;
  float grass_is_char_to_dirt_spawn_grow_rate = 0.1f;
  float grass_grow_speed = 0.01510f * high_chance_true;
  float fade_char_to_dirt_rate = 0.00001f;
  float fade_to_dry_rate = 0.0f; // sponge disabled, grass will never grow on dry soil and if this
  // is greater than the grass wetter delta then it will stay dry forever

  float fire_spread_chance = high_chance_true;

  //////////

  // left
  ivec2 sample_pixel = ivec2(p.x - 1, p.y);
  sample_pixel = clamp(sample_pixel, ivec2(0), ivec2(size - vec2(1)));
  vec4 other_sample = texelFetch(texture0, sample_pixel, 0).rgba;
  float other_water_height = other_sample.r;
  float other_ground_height = other_sample.g;
  float velocity_into_this_pixel_from_other = velocity.r;
  // vec4 left = calc_contribution2(heightmap,other_sample,velocity_into_this_pixel_from_other);
  vec4 left = calc_contribution2(
      ground_height, water_height, other_ground_height, other_water_height, velocity_into_this_pixel_from_other);

  ///////////////////////////////////////////////////////////////////////////////////
  float other_biome = other_sample.b;
  float other_grass_heavy = in_range(other_biome, 2.3f, 3.f);
  float grass_seed_spread = grass_seed_spread_speed * is_soil * other_grass_heavy * grass_seed_spread_chance; // adder

  if (bool(is_soil) && bool(grass_seed_spread_chance) && bool(other_grass_heavy))
  {
    biome = biome + grass_seed_spread_speed; // bad, makes black spots
    // biome = 2.1;
  }

  if (bool(grass_seed_char_to_grass) && bool(is_char_to_dirt) && bool(other_grass_heavy))
  {
    biome = biome + grass_seed_spread_speed; // bad, makes black spots
    // biome = 2.1;
  }
  // fire propagation:
  float other_fire_heavy = in_range(other_biome, 4.05f, 5.f);
  if (other_fire_heavy == 1.f && is_grass == 1.f)
  {
    if (bool(fire_spread_chance))
    {
      biome = 4.f;
    }
  }
  is_char_to_dirt = in_range(biome, 0.f, 1.f);
  is_soil = in_range(biome, 1.f, 2.f);
  is_very_wet_soil = in_range(biome, 1.5f, 2.f);
  is_grass = in_range(biome, 2.f, 3.f);
  on_fire = in_range(biome, 4.f, 5.f);
  ///////////////////////////////////////////////////////////////////////////////////

  // right
  sample_pixel = ivec2(p.x + 1, p.y);
  sample_pixel = clamp(sample_pixel, ivec2(0), ivec2(size - vec2(1)));
  other_sample = texelFetch(texture0, sample_pixel, 0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.g;
  vec4 right = calc_contribution2(
      ground_height, water_height, other_ground_height, other_water_height, velocity_into_this_pixel_from_other);

  ///////////////////////////////////////////////////////////////////////////////////
  other_biome = other_sample.b;
  other_grass_heavy = in_range(other_biome, 2.3f, 3.f);
  grass_seed_spread = grass_seed_spread_speed * is_soil * other_grass_heavy * grass_seed_spread_chance; // adder
  // biome = biome + grass_seed_spread;

  if (bool(is_soil) && bool(grass_seed_spread_chance) && bool(other_grass_heavy))
  {
    biome = biome + grass_seed_spread_speed;
    // biome = 2.1;
  }

  if (bool(grass_seed_char_to_grass) && bool(is_char_to_dirt) && bool(other_grass_heavy))
  {
    biome = biome + grass_seed_spread_speed;
    // biome = 2.1;
  }
  // fire propagation:
  other_fire_heavy = in_range(other_biome, 4.05f, 5.f);
  if (other_fire_heavy == 1.f && is_grass == 1.f)
  {
    if (bool(fire_spread_chance))
    {
      biome = 4.f;
    }
  }
  is_char_to_dirt = in_range(biome, 0.f, 1.f);
  is_soil = in_range(biome, 1.f, 2.f);
  is_very_wet_soil = in_range(biome, 1.5f, 2.f);
  is_grass = in_range(biome, 2.f, 3.f);
  on_fire = in_range(biome, 4.f, 5.f);
  ///////////////////////////////////////////////////////////////////////////////////

  // down
  sample_pixel = ivec2(p.x, p.y - 1);
  sample_pixel = clamp(sample_pixel, ivec2(0), ivec2(size - vec2(1)));
  other_sample = texelFetch(texture0, sample_pixel, 0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.b;
  // vec4 down = calc_contribution2(heightmap,other_sample,velocity_into_this_pixel_from_other);
  vec4 down = calc_contribution2(
      ground_height, water_height, other_ground_height, other_water_height, velocity_into_this_pixel_from_other);

  ///////////////////////////////////////////////////////////////////////////////////
  other_biome = other_sample.b;
  other_grass_heavy = in_range(other_biome, 2.3f, 3.f);
  grass_seed_spread = grass_seed_spread_speed * is_soil * other_grass_heavy * grass_seed_spread_chance; // adder
  // biome = biome + grass_seed_spread;

  if (bool(is_soil) && bool(grass_seed_spread_chance) && bool(other_grass_heavy))
  {
    biome = biome + grass_seed_spread_speed;
    // biome = 2.1;
  }

  if (bool(grass_seed_char_to_grass) && bool(is_char_to_dirt) && bool(other_grass_heavy))
  {
    biome = biome + grass_seed_spread_speed;
    // biome = 2.1;
  }
  // fire propagation:
  other_fire_heavy = in_range(other_biome, 4.05f, 5.f);
  if (other_fire_heavy == 1.f && is_grass == 1.f)
  {
    if (bool(fire_spread_chance))
    {
      biome = 4.f;
    }
  }
  is_char_to_dirt = in_range(biome, 0.f, 1.f);
  is_soil = in_range(biome, 1.f, 2.f);
  is_very_wet_soil = in_range(biome, 1.5f, 2.f);
  is_grass = in_range(biome, 2.f, 3.f);
  on_fire = in_range(biome, 4.f, 5.f);
  ///////////////////////////////////////////////////////////////////////////////////

  // up
  sample_pixel = ivec2(p.x, p.y + 1);
  sample_pixel = clamp(sample_pixel, ivec2(0), ivec2(size - vec2(1)));
  other_sample = texelFetch(texture0, sample_pixel, 0).rgba;
  other_water_height = other_sample.r;
  other_ground_height = other_sample.g;
  velocity_into_this_pixel_from_other = velocity.a;
  // vec4 up = calc_contribution2(heightmap,other_sample,velocity_into_this_pixel_from_other);
  vec4 up = calc_contribution2(
      ground_height, water_height, other_ground_height, other_water_height, velocity_into_this_pixel_from_other);

  ///////////////////////////////////////////////////////////////////////////////////
  other_biome = other_sample.b;
  other_grass_heavy = in_range(other_biome, 2.3f, 3.f);
  grass_seed_spread = grass_seed_spread_speed * is_soil * other_grass_heavy * grass_seed_spread_chance; // adder
  // biome = biome + grass_seed_spread;

  if (bool(is_soil) && bool(grass_seed_spread_chance) && bool(other_grass_heavy))
  {
    biome = biome + grass_seed_spread_speed;
    // biome = 2.1;
  }

  if (bool(grass_seed_char_to_grass) && bool(is_char_to_dirt) && bool(other_grass_heavy))
  {
    biome = biome + grass_seed_spread_speed;
    // biome = 2.1;
  }
  // fire propagation:
  other_fire_heavy = in_range(other_biome, 4.05f, 5.f);
  // float fire_want_spread =  other_fire_heavy * is_grass + other_fire_fading*is_grass;
  if (other_fire_heavy == 1.f && is_grass == 1.f)
  {
    if (bool(fire_spread_chance))
    {
      biome = 4.f;
    }
  }
  is_char_to_dirt = in_range(biome, 0.f, 1.f);
  is_soil = in_range(biome, 1.f, 2.f);
  is_very_wet_soil = in_range(biome, 1.5f, 2.f);
  is_grass = in_range(biome, 2.f, 3.f);
  on_fire = in_range(biome, 4.f, 5.f);
  ///////////////////////////////////////////////////////////////////////////////////

  // static iterations:
  if (bool(is_char_to_dirt))
  {
    biome = biome + fade_char_to_dirt_rate;
  }
  if (bool(is_grass))
  {
    biome = clamp(biome + grass_grow_speed, 2.f, 3.9f);
  }
  if (bool(is_very_wet_soil))
  {
    if (bool(wet_soil_grass_spawn_chance))
    {
      biome += grass_very_wet_soil_spawn_grow_rate;
    }
  }
  if (bool(is_soil))
  {
    if (bool(soil_grass_spawn_chance))
    {
      biome += grass_soil_spawn_grow_rate;
    }
  }

  if (bool(is_char_to_dirt))
  {
    if (bool(char_grass_spawn_chance))
    {
      biome += grass_is_char_to_dirt_spawn_grow_rate;
    }
  }

  if (bool(on_fire))
  {
    float fire_intensify = 0.3f * high_chance_true; // adder
    if (biome > 4.2)
    {
      fire_intensify = dt * 0.01171;
    }
    biome = biome + fire_intensify;
  }

  float velocity_sum = left.g + right.g + down.g + up.g;
  // water over ground:
  if (water_height > ground_height)
  {

    is_soil = in_range(biome, 1.f, 2.f);
    is_grass = float(biome > 2.f && biome < 4.f);
    is_char_to_dirt = in_range(biome, 0.f, 1.f);
    on_fire = in_range(biome, 4.f, 5.f);
    if (bool(is_soil))
    {
      biome = 1.99f;
    }

    if (bool(is_grass))
    {
      biome = biome - dt * 0.01f * abs(velocity_sum); // erode if fast
      biome = biome + dt * 0.001f;                    // grow grass
      biome = clamp(biome, 2.f, 3.9f);
    }

    if (bool(is_char_to_dirt))
    {
    }

    if (bool(on_fire))
    {
      if (water_height - ground_height > 0.01f)
      {
        float current_fire_intensity = biome - 4.f;
        biome = 1.f - current_fire_intensity - 0.5f;
        biome = clamp(biome, 0.f, 1.f);
      }
    }
  }

  if (!bool(in_range(biome, 0.f, 5.f)))
  {
    biome = 0.f;
  }
  if (bool(in_range(biome, 3.f, 4.f)))
  {
    biome = 3.f;
  }

  float delta_height_sum = (left.r + right.r + down.r + up.r);
  float updated_water_height = water_height + delta_height_sum;
  vec4 velocity_into_this_pixel_from_others = vec4(left.g, right.g, down.g, up.g);
  // could also do ground height erosion based on velocity^2 here
  out0 = vec4(updated_water_height, ground_height, biome, debug);
  out1 = velocity_into_this_pixel_from_others;
}