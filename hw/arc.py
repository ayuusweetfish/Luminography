from math import pi, sin, cos
r=46.65
a=15/180.*pi
# b=(-88.342-15)/180*pi
b=(-91.658-15)/180*pi
print('(start %f %f) (mid %f %f) (end %f %f)' % (100+r*cos(b), 100+r*sin(b), 100+r*cos(b+a/2), 100+r*sin(b+a/2), 100+r*cos(b+a), 100+r*sin(b+a)))
