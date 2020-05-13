#pragma once
#include "Globals.h"
#include "Render.h"
#include "Scene_Graph.h"
#include "glm/gtc/constants.hpp"

template <typename V, typename T> V lerp(V &v1, V &v2, T &t);

float lerp(float v1, float v2, float t);

template <typename V> struct Animation;

template <> struct Animation<float>
{
  Animation()
  {
    _v1 = 0.0f;
    _v2 = 1.0f;
    _current = 0.0f;
    _up = true;
    _t = 0.0f;
    _rate = 1.0f;
  }
  Animation(float v1, float v2, float rate = 1.0f)
  {
    _v1 = v1;
    _v2 = v2;
    _current = _v1;
    _up = true;
    _t = 0.0f;
    _rate = rate;
  }
  void reset()
  {
    _t = 0.0f;
    _current = _v1;
  }
  bool is_done() const
  {
    if (_up)
    {
      return _t == 1.0f;
    }
    if (!_up)
    {
      return _t == 0.0f;
    }
  }
  void step(float dt)
  {
    if (is_done())
    {
      return;
    }

    float blendrate = _rate * dt;

    if (_up)
    {
      _t = glm::clamp(_t + blendrate, 0.0f, 1.0f);
    }
    if (!_up)
    {
      _t = glm::clamp(_t - blendrate, 0.0f, 1.0f);
    }

    _current = lerp(_v1, _v2, _t);
  }
  void set_new_points(float v1, float v2)
  {
    _v1 = v1;
    _v2 = v2;
  }
  void flip_dir()
  {
    _up = !_up;
  }
  void set_dir(bool b)
  {
    _up = b;
  }
  float get_current() const
  {
    return _current;
  }
  float _rate;

private:
  float lerp(float v1, float v2, float t)
  {
    return v1 + t * (v2 - v1);
  }
  float _current;
  bool _up;
  float _t;
  float _v1;
  float _v2;
};

template <typename V> struct Animation
{
  Animation(V &v1, V &v2, float rate = 1.0f)
  {
    _v1 = v1;
    _v2 = v2;
    _current = _v1;
    _up = true;
    _t = 0.0f;
    _rate = rate;
  }
  bool is_done() const
  {
    if (_up)
    {
      return _t == 1.0f;
    }
    if (!_up)
    {
      return _t == 0.0f;
    }
  }
  template <typename V, typename T> V lerp(V &v1, V &v2, T &t)
  {
    return v1 + t * (v2 - v1);
  }
  void lerp(float dt)
  {
    if (is_done())
    {
      return;
    }

    float blendrate = _rate * dt;

    if (_up)
    {
      _t = clamp(_t + blendrate, 0.0f, 1.0f);
    }
    if (!_up)
    {
      _t = clamp(_t - blendrate, 0.0f, 1.0f);
    }

    _current = lerp(_v1, _v2, _t);
    _t = t;
  }
  void set_new_points(V &v1, V &v2)
  {
    _v1 = v1;
    _v2 = v2;
  }
  void flip_dir()
  {
    _up = !_up;
  }
  void set_dir(bool b)
  {
    _up = b;
  }
  bool peek_dir()
  {
    return _up;
  }
  V get_current() const
  {
    return _current;
  }
  float _rate;

private:
  V _current;
  bool _up;
  float _t;
  V _v1;
  V _v2;
};

//
// template <typename V> struct Animation;
//
// template <> struct Animation<float>
//{
//  Animation();
//  Animation(float v1, float v2, float rate = 1.0f);
//  void reset();
//  bool is_done();
//  const void step(float dt);
//  void set_new_points(float v1, float v2);
//  void flip_dir();
//  void set_dir(bool b);
//  float get_current();
//  const float _rate;
//
// private:
//  float lerp(float v1, float v2, float t);
//  float _current;
//  bool _up;
//  float _t;
//  float _v1;
//  float _v2;
//};
//
// template <typename V> struct Animation
//{
//  Animation(V &v1, V &v2, float rate = 1.0f);
//  bool is_done();
//  const
//
//      template <typename V, typename T>
//      V lerp(V &v1, V &v2, T &t);
//  void lerp(float dt);
//  void set_new_points(V &v1, V &v2);
//  void flip_dir();
//  void set_dir(bool b);
//  bool peek_dir();
//  V get_current();
//  const float _rate;
//
// private:
//  V _current;
//  bool _up;
//  float _t;
//  V _v1;
//  V _v2;
//};

struct Bezier_Curve
{
  Bezier_Curve() {}
  Bezier_Curve(std::vector<glm::vec4> pts);
  glm::vec4 lerp(float t);
  std::vector<glm::vec4> points;

private:
  std::vector<glm::vec4> remainder;
};

void fire_emitter(Renderer *renderer, Particle_Emitter *pe, Light *l, vec3 pos, vec2 size);
void fire_emitter2(Renderer *renderer, Flat_Scene_Graph* scene,Particle_Emitter *pe, Light *l, vec3 pos, vec2 size);