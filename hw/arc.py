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

  # x1, y1 = 99.25, 55.55
  # x2, y2 = 89.219938, 56.870482

  # delta1 = 0
  # delta2 = -0.03
  # x1, y1 = 105.058258, 144.167553
  # x2, y2 = 94.941699, 144.16762
  # delta1 = 0.04
  # delta2 = -0.03
  # x1, y1 = 105.156152, 144.911137
  # x2, y2 = 94.843805, 144.911204
  delta1 = 0
  # delta2 = -0.03
  # x1, y1 = 116.317321, 141.353392
  # x2, y2 = 106.545425, 143.971764
  delta2 = -0.04
  x1, y1 = 116.604333, 142.046302
  x2, y2 = 106.64332, 144.715347

  x1, y1 = x1-100, y1-100
  x2, y2 = x2-100, y2-100
  a1 = atan2(y1, x1) + delta1
  a2 = atan2(y2, x2) + delta2
  a0 = (a1+a2)/2
  r = sqrt(x1**2+y1**2)
  print('(start %f %f) (mid %f %f) (end %f %f)' % (
    100+r*cos(a1), 100+r*sin(a1),
    100+r*cos(a0), 100+r*sin(a0),
    100+r*cos(a2), 100+r*sin(a2),
  ))

# %s/(arc (start \(.\+\)) (mid \(.\+\)) (end \([0-9\.\- ]\+\)) \(.\+\)"B.Cu"\(.\+\)/(arc (start \1) (mid \2) (end \3) \4"F.Cu"\5\r  (via (at \1) (size 0.6) (drill 0.3) (layers "F.Cu" "B.Cu") (net 0) (tstamp 178c4af3-41cd-4a28-8580-311996084e20))\r (via  (at \3) (size 0.6) (drill 0.3) (layers "F.Cu" "B.Cu") (net 0) (tstamp 1 78c4af3-41cd-4a28-8580-311996084e20))/g
