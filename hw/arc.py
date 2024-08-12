from math import pi, sin, cos, sqrt, atan2

if False:
  r=46.65
  a=15/180.*pi
# b=(-88.342-15)/180*pi
  b=(-91.658-15)/180*pi
  print('(start %f %f) (mid %f %f) (end %f %f)' % (100+r*cos(b), 100+r*sin(b), 100+r*cos(b+a/2), 100+r*sin(b+a/2), 100+r*cos(b+a), 100+r*sin(b+a)))

if True:
  # x1, y1 = 99.25, 54.8
  # x2, y2 = 89.025823, 56.146038

  x1, y1 = 99.25, 55.55
  x2, y2 = 89.219938, 56.870482

  x1, y1 = x1-100, y1-100
  x2, y2 = x2-100, y2-100
  a1 = atan2(y1, x1)
  a2 = atan2(y2, x2)
  a0 = (a1+a2)/2
  r = sqrt(x1**2+y1**2)
  print('(start %f %f) (mid %f %f) (end %f %f)' % (
    100+x1, 100+y1,
    100+r*cos(a0), 100+r*sin(a0),
    100+x2, 100+y2,
  ))
