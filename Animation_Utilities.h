#pragma once
#include "Globals.h"

template <typename V, typename T> V lerp(V &v1, V &v2, T &t) { return v1 + t * (v2 - v1); }
float lerp(float v1, float v2, float t) { return v1 + t * (v2 - v1); }

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
  void flip_dir() { _up = !_up; }
  void set_dir(bool b) { _up = b; }
  float get_current() const { return _current; }
  float _rate;

private:
  float lerp(float v1, float v2, float t) { return v1 + t * (v2 - v1); }
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
  template <typename V, typename T> V lerp(V &v1, V &v2, T &t) { return v1 + t * (v2 - v1); }
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
  void flip_dir() { _up = !_up; }
  void set_dir(bool b) { _up = b; }
  bool peek_dir() { return _up; }
  V get_current() const { return _current; }
  float _rate;

private:
  V _current;
  bool _up;
  float _t;
  V _v1;
  V _v2;
};

struct Bezier_Curve
{
  Bezier_Curve() {}
  Bezier_Curve(std::vector<glm::vec4> pts) : points(pts) {}
  glm::vec4 lerp(float t)
  {
    if (remainder.size() == 0)
    {
      remainder = points;
    }
    if (remainder.size() == 1)
    {
      glm::vec4 p = remainder[0];
      remainder.clear();
      return p;
    }

    for (uint32 i = 0; i + 1 < remainder.size(); ++i)
    {
      glm::vec4 p = glm::mix(remainder[i], remainder[i + 1], t);
      remainder[i] = p;
    }
    remainder.pop_back();

    return lerp(t);
  }
  std::vector<glm::vec4> points;

private:
  std::vector<glm::vec4> remainder;
};