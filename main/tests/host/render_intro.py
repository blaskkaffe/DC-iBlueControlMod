#!/usr/bin/env python3
"""Renders the boot intro and the first seconds of the drops (what anim_intro_frames.c prints) as a video of
four blue port LEDs and the solid power LED, with a trace of the four duties under it.
python3 render_intro.py <out.mp4> <slow_factor> [show_ms]   (1 = real time, 4 = four times slower)"""
import subprocess, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

out = sys.argv[1]
slow = int(sys.argv[2]) if len(sys.argv) > 2 else 1
SHOW = int(sys.argv[3]) if len(sys.argv) > 3 else 5000
STEP = 1000 / (60 * slow) * slow  # sample every 1000/240 ms for slow, and 1000/60 for real time: see below
RATE = 240                         # samples per second of the data
rows = []
for line in subprocess.check_output(['/tmp/aif', '15000', '1000', str(SHOW), '4']).decode().split('\n'):
    if line.strip():
        p = line.split()
        rows.append((int(p[0]), [int(x) for x in p[1:5]]))
# 4 ms steps -> 250 samples/s; real time shows every 4th sample (60 fps), slow shows them all
frames = rows[::4] if slow == 1 else rows
fps = 60 if slow == 1 else 60
W, H = 1280, 720
try:
    font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', 30)
    small = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', 22)
except Exception:
    font = small = ImageFont.load_default()

def lin(duty):  # what the eye sees of a duty: the screen shows linear light with a 2.2 gamma
    return (duty / 1023.0) ** (1 / 2.2)

PX = [220, 450, 680, 910]
PY = 250
base = Image.new('RGB', (W, H), (18, 16, 15))
d = ImageDraw.Draw(base)
d.rounded_rectangle((90, 120, 1190, 380), 36, fill=(205, 198, 182))            # the console front
d.polygon([(565 - 40, 150), (565 + 40, 150), (565, 190)], fill=(60, 52, 40))   # power LED seat
for i, x in enumerate(PX):
    d.rounded_rectangle((x - 70, PY - 55, x + 70, PY + 55), 16, fill=(30, 28, 27))
    d.text((x - 8, PY - 100), 'ABCD'[i], font=small, fill=(90, 85, 75))
# the power LED: solid orange
power = Image.new('RGB', (W, H), (0, 0, 0))
pd = ImageDraw.Draw(power)
pd.polygon([(565 - 34, 154), (565 + 34, 154), (565, 186)], fill=(255, 110, 20))
power_glow = power.filter(ImageFilter.GaussianBlur(14))

ffmpeg = subprocess.Popen(['ffmpeg', '-v', 'error', '-y', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{W}x{H}',
                           '-r', str(fps), '-i', '-', '-an', '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '18', out],
                          stdin=subprocess.PIPE)
chart = (90, 440, 1190, 690)
total_ms = SHOW
for fi, (ms, duties) in enumerate(frames):
    img = base.copy()
    glow = Image.new('RGB', (W, H), (0, 0, 0))
    gd = ImageDraw.Draw(glow)
    for i, x in enumerate(PX):
        v = lin(duties[i])
        if v > 0.003:
            gd.rounded_rectangle((x - 62, PY - 47, x + 62, PY + 47), 12, fill=(int(40 * v), int(70 * v), int(255 * v)))
    soft = glow.filter(ImageFilter.GaussianBlur(26))
    arr = np.asarray(img).astype(np.int32) + np.asarray(glow).astype(np.int32) + (np.asarray(soft).astype(np.int32) * 0.8).astype(np.int32)
    arr += np.asarray(power).astype(np.int32) + (np.asarray(power_glow).astype(np.int32) * 0.5).astype(np.int32)
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8))
    dr = ImageDraw.Draw(img)
    dr.text((90, 40), 'port LEDs: fireworks, then the drops' + ('' if slow == 1 else '   (%dx slower)' % slow), font=font, fill=(235, 235, 235))
    dr.text((90, 85), '%.2f s' % (ms / 1000.0), font=small, fill=(180, 180, 180))
    # the traces of A..D, with a cursor
    dr.rectangle(chart, fill=(26, 24, 23))
    rowh = (chart[3] - chart[1]) / 4
    for i in range(4):
        y0 = chart[1] + i * rowh
        dr.text((chart[0] + 6, y0 + 4), 'ABCD'[i], font=small, fill=(130, 130, 130))
        pts = []
        for (t, dd) in rows[::2]:
            x = chart[0] + 40 + (t / total_ms) * (chart[2] - chart[0] - 50)
            y = y0 + rowh - 8 - lin(dd[i]) * (rowh - 20)
            pts.append((x, y))
        dr.line(pts, fill=(90, 130, 230), width=2)
    cx = chart[0] + 40 + (ms / total_ms) * (chart[2] - chart[0] - 50)
    dr.line((cx, chart[1], cx, chart[3]), fill=(255, 200, 80), width=2)
    ffmpeg.stdin.write(img.tobytes())
ffmpeg.stdin.close()
ffmpeg.wait()
print(out, len(frames), 'frames')
