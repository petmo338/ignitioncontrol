# main.py -- put your code here!
from pyb import udelay, Pin

pin = Pin('X1', pyb.Pin.OUT_PP)
HIGHTIME = 6000
POSTIME = 1000
LOWTIME = 800

while True:
    pyb.udelay(HIGHTIME)
    pin.low()
    pyb.udelay(LOWTIME)
    pin.high()
    
    pyb.udelay(POSTIME)
    pin.low()
    pyb.udelay(LOWTIME)
    pin.high()
    
    pyb.udelay(HIGHTIME - POSTIME - LOWTIME)
    pin.low()
    pyb.udelay(LOWTIME)
    pin.high()
    
    pyb.udelay(HIGHTIME)
    pin.low()
    pyb.udelay(LOWTIME)
    pin.high()
    
    pyb.udelay(HIGHTIME)
    pin.low()
    pyb.udelay(LOWTIME)
    pin.high()
    