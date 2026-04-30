# amoled.py — Driver RM67162 utilisant amoled_qspi (module C natif)
# Couleurs RGB565 big-endian
import amoled_qspi, time
from machine import Pin

BLACK   = 0x0000; WHITE   = 0xFFFF
RED     = 0xF800; GREEN   = 0x07E0
BLUE    = 0x001F; CYAN    = 0x07FF
YELLOW  = 0xFFE0; MAGENTA = 0xF81F
ORANGE  = 0xFC00

_W = 536; _H = 240

class AMOLED:

    def __init__(self, freq=80_000_000,
                 host=2, sck=47, d0=18, d1=7, d2=48, d3=5,
                 cs=6, rst=17, en=38):

        Pin(en, Pin.OUT).value(1)
        time.sleep_ms(50)

        self._bus = amoled_qspi.QSPIBus(
            host=host, sck=sck, d0=d0, d1=d1, d2=d2, d3=d3,
            cs=cs, freq=freq
        )
        self._rst = Pin(rst, Pin.OUT)
        self.width  = _W
        self.height = _H

        self._reset()
        self._init()

    def _reset(self):
        self._rst.value(1); time.sleep_ms(50)
        self._rst.value(0); time.sleep_ms(300)
        self._rst.value(1); time.sleep_ms(200)

    def _cmd(self, reg, data=None):
        self._bus.tx_param(reg, bytes(data) if data else bytes(0))

    def _init(self):
        self._cmd(0x11); time.sleep_ms(120)
        self._cmd(0x3A, [0x55])   # RGB565
        self._cmd(0x51, [0x00])
        self._cmd(0x29); time.sleep_ms(120)
        self._cmd(0x51, [0xD0])   # brightness
        self._cmd(0x36, [0x60])   # MADCTL paysage
        print("AMOLED OK " + str(self.width) + "x" + str(self.height))

    def brightness(self, val):
        self._cmd(0x51, [int(val) & 0xFF])

    def colorRGB(self, r, g, b):
        return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

    def set_window(self, x1, y1, x2, y2):
        self._cmd(0x2A, [x1>>8, x1&0xFF, x2>>8, x2&0xFF])
        self._cmd(0x2B, [y1>>8, y1&0xFF, y2>>8, y2&0xFF])

    def fill(self, color):
        hi = (color >> 8) & 0xFF
        lo =  color        & 0xFF
        self.set_window(0, 0, _W-1, _H-1)
        line = bytearray([hi, lo] * _W)
        self._bus.tx_color(0, line)
        for _ in range(_H - 1):
            self._bus.tx_pixels(0, line)
        self._bus.tx_end()

    def fill_rect(self, x, y, w, h, color):
        if w <= 0 or h <= 0: return
        hi = (color >> 8) & 0xFF
        lo =  color        & 0xFF
        self.set_window(x, y, x+w-1, y+h-1)
        line = bytearray([hi, lo] * w)
        self._bus.tx_color(0, line)
        for _ in range(h - 1):
            self._bus.tx_pixels(0, line)
        self._bus.tx_end()

    def pixel(self, x, y, color):
        hi = (color >> 8) & 0xFF
        lo =  color        & 0xFF
        self.set_window(x, y, x, y)
        self._bus.tx_color(0, bytearray([hi, lo]))
        self._bus.tx_end()

    def hline(self, x, y, w, color): self.fill_rect(x, y, w, 1, color)
    def vline(self, x, y, h, color): self.fill_rect(x, y, 1, h, color)

    def rect(self, x, y, w, h, color):
        self.hline(x, y, w, color)
        self.hline(x, y+h-1, w, color)
        self.vline(x, y, h, color)
        self.vline(x+w-1, y, h, color)

    def fill_circle(self, cx, cy, r, color):
        for dy in range(-r, r+1):
            dx = int((r*r - dy*dy)**0.5)
            self.hline(cx-dx, cy+dy, 2*dx+1, color)

    def circle(self, cx, cy, r, color):
        x, y, d = 0, r, 1-r
        while x <= y:
            for px, py in [(cx+x,cy+y),(cx-x,cy+y),(cx+x,cy-y),(cx-x,cy-y),
                           (cx+y,cy+x),(cx-y,cy+x),(cx+y,cy-x),(cx-y,cy-x)]:
                if 0<=px<_W and 0<=py<_H: self.pixel(px, py, color)
            if d < 0: d += 2*x+3
            else: d += 2*(x-y)+5; y -= 1
            x += 1

    def blit(self, x, y, w, h, buf):
        self.set_window(x, y, x+w-1, y+h-1)
        self._bus.tx_color(0, buf)
        self._bus.tx_end()

    def text(self, font, s, x, y, fg=WHITE, bg=BLACK):
        fw = font.WIDTH  if hasattr(font, 'WIDTH')  else 16
        fh = font.HEIGHT if hasattr(font, 'HEIGHT') else 32
        fdata = font._FONT
        fg_hi=(fg>>8)&0xFF; fg_lo=fg&0xFF
        bg_hi=(bg>>8)&0xFF; bg_lo=bg&0xFF
        bpr = (fw+7)//8
        fg_px = bytearray([fg_hi, fg_lo])
        bg_px = bytearray([bg_hi, bg_lo])
        # Buffer pour un caractère complet
        char_buf = bytearray(fw * fh * 2)
        for ch in s:
            ci = ord(ch)
            if ci < 0x20 or ci > 0x7F: ci = 0x20
            idx = 0
            for row in range(fh):
                base = (ci*fh + row)*bpr
                for col in range(fw):
                    if (fdata[base+col//8] >> (7-col%8)) & 1:
                        char_buf[idx]   = fg_hi
                        char_buf[idx+1] = fg_lo
                    else:
                        char_buf[idx]   = bg_hi
                        char_buf[idx+1] = bg_lo
                    idx += 2
            self.set_window(x, y, x+fw-1, y+fh-1)
            self._bus.tx_color(0, char_buf)
            self._bus.tx_end()
            x += fw