import time
import mmap
import os
import re
from Adafruit_PCA9685 import PCA9685

# 共有メモリの設定
SHM_NAME = "/dev/shm/propeller_data"
SHM_SIZE = 256

# ESCのPWMパラメータ
ESC_REV = 300      # 最大逆回転
ESC_NEUTRAL = 400  # 停止
ESC_FWD = 500      # 最大前進
ESC_CHANNELS = [0, 1]

# PWM変化制限
MAX_DELTA = 20

# 初期状態
current_pwm = [ESC_NEUTRAL, ESC_NEUTRAL]

def validate_pwm(value):
    return max(ESC_REV, min(ESC_FWD, value))

def limit_pwm_change(new, current):
    if abs(new - current) > MAX_DELTA:
        return current + MAX_DELTA if new > current else current - MAX_DELTA
    return new

def parse_pwm_data(data_str):
    try:
        # 角括弧ありの形式に対応
        match = re.match(r"\[(\d+),\s*(\d+)\]", data_str.strip())
        if not match:
            return None
        left = validate_pwm(int(match.group(1)))
        right = validate_pwm(int(match.group(2)))
        return [left, right]
    except Exception as e:
        print(f"parse_pwm_data error: {e}")
        return None

def main():
    # PCA9685 初期化
    pwm = PCA9685()
    pwm.set_pwm_freq(60)

    # ESC アーム処理
    print("Arming ESCs...")
    for ch in ESC_CHANNELS:
        pwm.set_pwm(ch, 0, ESC_NEUTRAL)
    time.sleep(2)

    # 共有メモリオープン
    fd = os.open(SHM_NAME, os.O_RDONLY)
    shm = mmap.mmap(fd, SHM_SIZE, mmap.MAP_SHARED, mmap.PROT_READ)

    print("プロペラ制御開始")

    global current_pwm

    while True:
        try:
            shm.seek(0)
            raw = shm.read(SHM_SIZE).rstrip(b'\x00').decode()
            pwm_values = parse_pwm_data(raw)

            if pwm_values:
                for i in range(2):
                    safe_pwm = limit_pwm_change(pwm_values[i], current_pwm[i])
                    pwm.set_pwm(ESC_CHANNELS[i], 0, safe_pwm)
                    current_pwm[i] = safe_pwm

                print(f"PWM sent: L={current_pwm[0]}, R={current_pwm[1]}")

            time.sleep(0.1)

        except KeyboardInterrupt:
            print("停止します...")
            break
        except Exception as e:
            print(f"エラー: {e}")
            time.sleep(0.5)

    for ch in ESC_CHANNELS:
        pwm.set_pwm(ch, 0, ESC_NEUTRAL)
    print("ESC停止完了")

if __name__ == "__main__":
    main()

