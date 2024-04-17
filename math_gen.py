import math
import struct

def to_fixed(val: float) -> int:
    return max(-32767 * 65536, min(32767 * 65536, round(val * 65536)))

angle_count = 128
angle_360 = angle_count
angle_180 = angle_count // 2

screen_width = 320
screen_height = 256
atan2_scale = 8

with open("build/data/game_math.dat", "wb") as file_out:
    # tan
    for y in range(screen_height // atan2_scale):
        for x in range(screen_width // atan2_scale):
            catan = round(angle_180 * math.atan2(y, x) / math.pi)
            if catan >= angle_360:
                catan -= angle_360
            file_out.write(struct.pack(">h", catan))

    # sin
    for i in range(angle_count):
        rads = 2 * math.pi * i / angle_count
        csin = math.sin(rads)
        fixsin = to_fixed(csin)
        print("brad {} ang {} sin {} fix {}".format(i, math.degrees(rads), csin, fixsin))
        file_out.write(struct.pack(">i", fixsin))


