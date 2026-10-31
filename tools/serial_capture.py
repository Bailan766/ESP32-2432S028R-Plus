"""串口抓取工具。

用法:
    python tools/serial_capture.py COM9 25            # 先硬复位再抓 25 秒
    python tools/serial_capture.py COM9 25 noreset    # 不复位，直接抓当前输出
"""
import serial, time, sys

port = sys.argv[1] if len(sys.argv) > 1 else 'COM9'
dur = float(sys.argv[2]) if len(sys.argv) > 2 else 25.0
do_reset = not (len(sys.argv) > 3 and sys.argv[3] == 'noreset')

s = serial.Serial(port, 115200, timeout=0.3)
if do_reset:
    # 复位 ESP32 (CH340: RTS->EN, DTR->GPIO0)
    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.15)
    s.setRTS(False)
    time.sleep(0.3)

end = time.time() + dur
buf = []
while time.time() < end:
    data = s.read(4096)
    if data:
        buf.append(data)
s.close()

out = b''.join(buf).decode('utf-8', errors='replace')
print(out)
