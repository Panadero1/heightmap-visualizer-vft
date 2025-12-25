slope_resolution = 240.0
g_bot = 1.375
g_veh = 0.498
scale_down = 0.70


def print_segment(start, end, startcol, endcol, f):
  res = str(start) + " " + str((end + start) / 2.0) + " " + str(end) + " "

  res += str(startcol[0])+" "+str(startcol[1])+" "+str(startcol[2])+" 1.0 "

  res += str(endcol[0])+" "+str(endcol[1])+" "+str(endcol[2])+" 1.0 "

  res += "1 0 0 0"

  print(res, file=f)

def fun(x):
  return min(1.0, (x * slope_resolution * scale_down) / 1000.0)

with open("zk-pathing.ggr", "w") as f:
  print("GIMP Gradient", file=f)
  print("Name: ZK-pathing", file=f)
  print("3", file=f)

  veh_slope = fun(g_veh)
  bot_slope = fun(g_bot)

  print_segment(0.0, veh_slope, [0.2, 0.2, 0.0], [1.0, 1.0, 0.0], f);
  print_segment(veh_slope, bot_slope, [0.0, 0.5, 0.5], [0.0, 1.0, 1.0], f);
  print_segment(bot_slope, 1.0, [0.5, 0.0, 0.5], [1.0, 0.0, 1.0], f);
