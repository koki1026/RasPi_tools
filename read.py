import mmap
import os
import time

fd = os.open("/dev/shm/propeller_shm", os.O_RDONLY)
shm = mmap.mmap(fd, 256, mmap.MAP_SHARED, mmap.PROT_READ)

while True:
    shm.seek(0)
    print("Shared:", shm.read(256).rstrip(b'\x00').decode())
    time.sleep(0.1)
