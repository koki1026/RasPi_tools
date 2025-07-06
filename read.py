import mmap
import os
import time

# 共有メモリファイルの読み取り専用オープン
fd = os.open("/dev/shm/propeller_data", os.O_RDONLY)
fd2 = os.open("/dev/shm/emergency_data", os.O_RDONLY)

# mmapでマッピング（共有・読み取り専用）
shm = mmap.mmap(fd, 256, mmap.MAP_SHARED, mmap.PROT_READ)
shm2 = mmap.mmap(fd2, 256, mmap.MAP_SHARED, mmap.PROT_READ)

print("監視を開始します... Ctrl+Cで停止")

try:
    while True:
        shm.seek(0)
        prop = shm.read(256).rstrip(b'\x00').decode()

        shm2.seek(0)
        emerg = shm2.read(256).rstrip(b'\x00').decode()

        print(f"[Propeller] {prop} | [Emergency] {emerg}")
        time.sleep(0.1)
except KeyboardInterrupt:
    print("監視を終了しました。")

# 後始末
shm.close()
shm2.close()
os.close(fd)
os.close(fd2)
