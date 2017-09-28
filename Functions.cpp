#include "Functions.h"
using namespace glm;
bool intersects(vec3 pa, vec3 da, vec3 pb, vec3 db)
{
  float32 axn, axm, ayn, aym, azn, azm, bxn, bxm, byn, bym, bzn, bzm;

  axn = pa.x - 0.5f * da.x;
  axm = pa.x + 0.5f * da.x;
  ayn = pa.y - 0.5f * da.y;
  aym = pa.y + 0.5f * da.y;
  azn = pa.z - 0.5f * da.z;
  azm = pa.z + 0.5f * da.z;
  bxn = pb.x - 0.5f * db.x;
  bxm = pb.x + 0.5f * db.x;
  byn = pb.y - 0.5f * db.y;
  bym = pb.y + 0.5f * db.y;
  bzn = pb.z - 0.5f * db.z;
  bzm = pb.z + 0.5f * db.z;

  return (axn <= bxm && axm >= bxn) && (ayn <= bym && aym >= byn) &&
    (azn <= bzm && azm >= bzn);
}

bool intersects(vec3 pa, Cylinder ca, vec3 pb, Cylinder cb)
{
  bool xyin = distance(vec2(pa.x, pa.y), vec2(pb.x, pb.y)) < (ca.r + cb.r);

  bool zin = (((pa.z + ca.h / 2) >= (pb.z - cb.h / 2)) &&
    ((pa.z + ca.h / 2) <= (pb.z + cb.h / 2))) ||
    (((pb.z + cb.h / 2) >= (pa.z - ca.h / 2)) &&
    ((pb.z + cb.h / 2) <= (pa.z + ca.h / 2)));

  return xyin && zin;
}

float32 distToSegment(vec2 p, vec2 v, vec2 w)
{
  float32 l2 = pow(v.x - w.x, 2) + pow(v.y - w.y, 2);
  if (l2 == 0)
  {
    return sqrt(distance(v, w));
  }
  float32 t = ((p.x - v.x) * (w.x - v.x) + (p.y - v.y) * (w.y - v.y)) / l2;
  t = max((float32)0.0, min((float32)1.0, t));
  return sqrt(distance(p, vec2{ v.x + t * (w.x - v.x), v.y + t * (w.y - v.y) }));
}

bool intersects(vec3 pa, Cylinder ca, Wall w)
{
  // bool zin = (((pa.z + ca.h / 2) >= (pb.z - cb.h / 2)) &&
  //             ((pa.z + ca.h / 2) <= (pb.z + cb.h / 2))) ||
  //            (((pb.z + cb.h / 2) >= (pa.z - ca.h / 2)) &&
  //             ((pb.z + cb.h / 2) <= (pa.z + ca.h / 2)));
  bool zin = true;

  vec2 p = { pa.x, pa.y };
  vec2 v = { w.p1.x, w.p1.y };

  bool xyin = distToSegment(p, v, w.p2) < ca.r;

  return zin && xyin;
}
