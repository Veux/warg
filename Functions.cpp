#include "Functions.h"

using namespace glm;

float32 distToSegment(vec2 p, vec2 v, vec2 w)
{
  float32 l2 = pow(v.x - w.x, 2) + pow(v.y - w.y, 2);
  if (l2 == 0)
  {
    return sqrt(distance(v, w));
  }
  float32 t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
  t = max((float32)0.0, min((float32)1.0, t));
  return sqrt(distance(p, vec2{v.x + t * (w.x - v.x), v.y + t * (w.y - v.y)}));
}

bool intersects(Segment pq, Triangle abc, vec3 &uvw, float32 &t)
{
  float32 &u = uvw.x;
  float32 &v = uvw.y;
  float32 &w = uvw.z;

  vec3 ab = abc.b - abc.a;
  vec3 ac = abc.c - abc.a;
  vec3 qp = pq.p - pq.q;

  vec3 n = cross(ab, ac);

  float32 d = dot(qp, n);
  if (d <= 0.0f)
    return false;

  vec3 ap = pq.p - abc.a;
  t = dot(ap, n);
  if (t < 0.0f)
    return false;
  if (t > d)
    return false;

  vec3 e = cross(qp, ap);
  v = dot(ac, e);
  if (v < 0.0f || v > d)
    return 0;
  w = -dot(ab, e);
  if (w < 0.0f || v + w > d)
    return 0;

  float ood = 1.0f / d;
  t *= ood;
  v *= ood;
  w *= ood;
  u = 1.0f - v - w;
  return true;
}

bool intersects(Sphere s, Triangle abc, vec3 &p)
{
  p = closest_point(s.c, abc);
  vec3 v = p - s.c;
  return dot(v, v) <= s.r * s.r;
}

vec3 closest_point(vec3 p, Triangle abc)
{
  vec3 &a = abc.a;
  vec3 &b = abc.b;
  vec3 &c = abc.c;

  vec3 ab = b - a;
  vec3 ac = c - a;
  vec3 bc = c - b;

  float32 snom = dot(p - a, ab);
  float32 sdenom = dot(p - b, a - b);

  float32 tnom = dot(p - a, ac);
  float32 tdenom = dot(p - c, a - c);

  if (snom <= 0.0f && tnom <= 0.0f)
    return abc.a;

  float32 unom = dot(p - b, bc);
  float32 udenom = dot(p - c, b - c);

  if (sdenom <= 0.0f && unom <= 0.0f)
    return abc.b;
  if (tdenom <= 0.0f && udenom <= 0.0f)
    return abc.c;

  vec3 n = cross(b - a, c - a);
  float32 vc = dot(n, cross(a - p, b - p));
  if (vc <= 0.0f && snom >= 0.0f && sdenom >= 0.0f)
    return a + snom / (snom + sdenom) * ab;

  float32 va = dot(n, cross(b - p, c - p));
  if (va <= 0.0f && unom >= 0.0f && udenom >= 0.0f)
    return b + unom / (unom + udenom) * bc;

  float32 vb = dot(n, cross(c - p, a - p));

  if (vb <= 0.0f && tnom >= 0.0f && tdenom >= 0.0f)
    return a + tnom / (tnom + tdenom) * ac;

  float32 u = va / (va + vb + vc);
  float32 v = vb / (va + vb + vc);
  float32 w = 1.0f - u - v;
  return u * a + v * b + w * c;
}

bool intersects(Barbell b, Triangle abc) {
	vec3 v;
	float32 f;

	if (intersects(b.pq, abc, v, f))
		return true;

	Sphere s;
	s.r = b.r;

	s.c = b.pq.p;
	if (intersects(s, abc, v))
		return true;

	s.c = b.pq.q;
	if (intersects(s, abc, v))
		return true;

	return false;
}
