from machine import Pin
import utime
#精度に関してはモーターの駆動音の周波数から割り出したものとほぼ一致しているため心配なし

# ピンの設定
rDisk = Pin(22, Pin.IN)
lDisk = Pin(26, Pin.IN)

# 変数の初期化
r_last_time = 0
l_last_time = 0
r_diff = 0
l_diff = 0

# 右ディスクの割り込みハンドラ
def r_callback(pin):
    global r_last_time, r_diff
    current = utime.ticks_us() # より高精度なマイクロ秒を使用
    if utime.ticks_diff(current, r_last_time) > 2000:
        r_diff = utime.ticks_diff(current, r_last_time)
        r_last_time = current

# 左ディスクの割り込みハンドラ
def l_callback(pin):
    global l_last_time, l_diff
    current = utime.ticks_us()
    if utime.ticks_diff(current, l_last_time) > 2000:
        l_diff = utime.ticks_diff(current, l_last_time)
        l_last_time = current

# 立ち上がりエッジ（False -> True）で関数を実行するように設定
rDisk.irq(trigger=Pin.IRQ_RISING, handler=r_callback)
lDisk.irq(trigger=Pin.IRQ_RISING, handler=l_callback)

def calculateSpeed():
    global r_diff, l_diff
    # 右側の計算 (RPM: 1分間あたりの回転数)
    if r_diff > 0:
        # 1秒あたりの回転数(Hz) = 1,000,000 / r_diff (us)
        r_rps = 1000000 / r_diff
        r_rpm = r_rps * 60
    else:
        r_rpm = 0

    # 左側の計算
    if l_diff > 0:
        l_rps = 1000000 / l_diff
        l_rpm = l_rps * 60
    else:
        l_rpm = 0

    #print("Right: {:.1f} RPM, Left: {:.1f} RPM".format(r_rpm, l_rpm))
        
    # 停止判定（1秒以上信号がなければ0とする）
    if utime.ticks_diff(utime.ticks_us(), r_last_time) > 1000000:
        r_diff = 0
    if utime.ticks_diff(utime.ticks_us(), l_last_time) > 1000000:
        l_diff = 0
        
    return r_rpm, l_rpm



