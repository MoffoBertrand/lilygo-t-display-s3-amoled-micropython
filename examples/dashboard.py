# main.py — Dashboard LILYGO T-Display S3 AMOLED Plus
# Firmware lvgl_micropython + module amoled_qspi C natif
import sys, time, math, machine
if '/lib' not in sys.path:
    sys.path.insert(0, '/lib')

from amoled import AMOLED, BLACK, WHITE, RED, GREEN, BLUE, CYAN, YELLOW, MAGENTA

display = AMOLED()
display.brightness(200)

# Police
try:
    import vga2_bold_16x32 as font
    HAS_FONT = True
    FW, FH = font.WIDTH, font.HEIGHT
    print("Police OK")
except:
    HAS_FONT = False; FW, FH = 8, 8

def rgb(r,g,b): return display.colorRGB(r,g,b)
DB = rgb(10,40,100); MB = rgb(0,100,200)
GY = rgb(60,60,60);  OR = rgb(255,140,0)

def txt(s, x, y, fg=WHITE, bg=BLACK):
    if HAS_FONT: display.text(font, s, x, y, fg, bg)

# Touch
_i2c = machine.I2C(0, sda=machine.Pin(3), scl=machine.Pin(2), freq=400_000)
_irq = machine.Pin(21, machine.Pin.IN, machine.Pin.PULL_UP)
print("Touch: " + str([hex(d) for d in _i2c.scan()]))

def get_touch():
    if _irq.value() == 1: return None
    try:
        _i2c.writeto(0x15, bytes([0x01]))
        d = _i2c.readfrom(0x15, 6)
        if d[1] > 0:
            return (((d[2]&0x0F)<<8)|d[3], ((d[4]&0x0F)<<8)|d[5])
    except: pass
    return None

W = display.width   # 536
H = display.height  # 240

# ── UI ────────────────────────────────────────────────────────
display.fill(BLACK)
display.fill_rect(0, 0, W, 42, DB)
display.hline(0, 42, W, MB)
display.rect(1, 1, W-2, H-2, MB)
display.fill_circle(W-18, 21, 7, GREEN)
txt("T-Display S3 AMOLED+", 10, 7, WHITE, DB)
display.fill_rect(6, 52, 4, FH, CYAN)
txt("Touches:", 14, 52, CYAN, BLACK)
display.fill_rect(6, 100, 4, FH, YELLOW)
txt("Luminosite:", 14, 100, YELLOW, BLACK)
display.fill_rect(6, 160, 4, FH, GREEN)
txt("Position:", 14, 160, GREEN, BLACK)
display.hline(0, 88, W, GY)
display.hline(0, 148, W, GY)
display.hline(0, 198, W, GY)
txt("Driver: amoled_qspi + lvgl_micropython", 10, H-FH-4, GY, BLACK)

count=[0]; bright=[200]; phase=[0]
lc=[-1]; lb=[-1]; lp=[None]

def draw_count(n):
    if n==lc[0]: return
    lc[0]=n
    display.fill_rect(14+9*FW, 52, W-20-9*FW, FH, BLACK)
    txt(str(n), 14+9*FW, 52, WHITE, BLACK)

def draw_bright(b):
    b=int(b)
    if abs(b-lb[0])<3: return
    lb[0]=b
    bw=int(b/255*(W-40))
    display.fill_rect(14, 128, W-28, 14, rgb(30,30,0))
    display.fill_rect(14, 128, bw,   14, rgb(255,200,0))
    display.fill_rect(W-50, 100, 48, FH, BLACK)
    txt(str(int(b/255*100))+"%", W-50, 100, YELLOW, BLACK)

def draw_pos(t):
    if t==lp[0]: return
    lp[0]=t
    display.fill_rect(14, 160, W-20, FH+4, BLACK)
    if t:
        txt("x="+str(t[0])+" y="+str(t[1]), 14, 160, GREEN, BLACK)
        px=min(max(t[0],5),W-5)
        py=min(max(t[1],5),H-5)
        display.fill_circle(px, py, 6, RED)
        display.circle(px, py, 12, OR)
    else:
        txt("aucun toucher", 14, 160, GY, BLACK)

draw_count(0); draw_bright(200); draw_pos(None)
print("Pret! W=" + str(W) + " H=" + str(H))

import utime
t0=utime.ticks_ms(); t_clr=[utime.ticks_ms()+60000]; last_=[None]

while True:
    now=utime.ticks_ms()
    t=get_touch()
    if t and t!=last_[0]:
        last_[0]=t; count[0]+=1
        draw_count(count[0]); draw_pos(t)
        t_clr[0]=now+2000
    elif not t:
        last_[0]=None
        if utime.ticks_diff(now,t_clr[0])>0:
            draw_pos(None); t_clr[0]=now+60000
    if utime.ticks_diff(now,t0)>=50:
        t0=now; phase[0]=(phase[0]+2)%360
        bright[0]=int(150+55*math.sin(math.radians(phase[0])))
        display.brightness(int(bright[0])); draw_bright(bright[0])
    time.sleep_ms(15)
