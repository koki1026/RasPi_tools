#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <pigpio.h>

#define SHM_NAME "/emergency_data"
#define SHM_SIZE 256
#define RELAY_PIN 17

int main() {
    // pigpio 初期化
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio initialization failed." << std::endl;
        return 1;
    }

    gpioSetMode(RELAY_PIN, PI_OUTPUT);
    gpioWrite(RELAY_PIN, PI_LOW); // 初期状態はOFF

    // 共有メモリを開く
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1) {
        std::cerr << "Failed to open shared memory: " << SHM_NAME << std::endl;
        gpioTerminate();
        return 1;
    }

    void* shm_ptr = mmap(nullptr, SHM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) {
        std::cerr << "mmap failed." << std::endl;
        close(shm_fd);
        gpioTerminate();
        return 1;
    }

    std::string last_message;
    auto last_heartbeat_time = std::chrono::steady_clock::now();

    std::cout << "Monitoring emergency_data shared memory..." << std::endl;

    while (true) {
        // 読み取り
        char buffer[SHM_SIZE] = {0};
        memcpy(buffer, shm_ptr, SHM_SIZE - 1);
        std::string message(buffer);

        // メッセージが変化した場合にのみ処理
        if (!message.empty() && message != last_message) {
            last_message = message;

            if (message == "Start") {
                gpioWrite(RELAY_PIN, PI_HIGH);
                std::cout << "[RELAY] ON (Start received)\n";
            } else if (message == "Stop") {
                gpioWrite(RELAY_PIN, PI_LOW);
                std::cout << "[RELAY] OFF (Stop received)\n";
            } else if (message == "True") {
                // Heartbeat
                last_heartbeat_time = std::chrono::steady_clock::now();
                std::cout << "[Heartbeat] Received\n";
            } else {
                std::cout << "[Unknown] Received message: " << message << std::endl;
            }
        }

        // Heartbeat監視（5秒以上来てないとOFFに）
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_heartbeat_time).count();
        if (elapsed > 5) {
            gpioWrite(RELAY_PIN, PI_LOW);
            std::cerr << "[Timeout] No Heartbeat for " << elapsed << " sec → RELAY OFF\n";
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 終了処理（通常は到達しない）
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);
    gpioTerminate();
    return 0;
}
