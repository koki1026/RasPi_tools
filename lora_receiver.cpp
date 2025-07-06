// LoRaからのシリアルデータを読み取り、2つの共有メモリに分けて書き込む：
// ・/dev/shm/propeller_data → [PWM_L, PWM_R]
// ・/dev/shm/emergency_data → "Start" / "Stop" / "True"

#include <iostream>
#include <string>
#include <sstream>
#include <regex>
#include <algorithm>
#include <thread>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <errno.h>
#include <sys/mman.h>

#define PROP_SHM "/dev/shm/propeller_data"
#define EMER_SHM "/dev/shm/emergency_data"
#define SHM_SIZE 256

int open_serial_port(const char* device, speed_t baud_rate) {
    int serial_port = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_port < 0) {
        std::cerr << "Error opening serial port: " << strerror(errno) << std::endl;
        return -1;
    }
    fcntl(serial_port, F_SETFL, 0);

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(serial_port, &tty) != 0) {
        std::cerr << "Error from tcgetattr: " << strerror(errno) << std::endl;
        return -1;
    }

    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);

    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;

    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        std::cerr << "Error from tcsetattr: " << strerror(errno) << std::endl;
        return -1;
    }

    return serial_port;
}

std::string read_serial_port(int serial_port) {
    char buf[256];
    memset(buf, 0, sizeof(buf));
    ssize_t n = read(serial_port, buf, sizeof(buf) - 1);
    if (n > 0) {
        return std::string(buf, n);
    }
    return "";
}

void write_to_shm(const char* path, const std::string& msg) {
    int fd = shm_open(path, O_CREAT | O_RDWR, 0666);
    if (fd == -1) return;
    ftruncate(fd, SHM_SIZE);
    void* ptr = mmap(nullptr, SHM_SIZE, PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) return;
    memset(ptr, 0, SHM_SIZE);
    strncpy((char*)ptr, msg.c_str(), SHM_SIZE - 1);
    munmap(ptr, SHM_SIZE);
    close(fd);
}

std::string parse_message(const std::string& raw) {
    std::regex re("@-?\\d+,-?\\d+,(.*)");
    std::smatch m;
    if (std::regex_search(raw, m, re) && m.size() > 1) {
        return m[1].str();
    }
    return "";
}

int main() {
    const char* device = "/dev/ttyUSB0";
    int port = open_serial_port(device, B115200);
    if (port < 0) return 1;

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    write(port, "Own = 6\r\n", 9);
    std::cout << "Sent: Own = 6\\r\\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    write(port, "Dst = 5\r\n", 9);
    std::cout << "Sent: Dst = 5\\r\\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
     //確認コマンド
    write(port, "#?\r\n", 5);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    write(port, "RECV -1\r\n", 10);
    std::cout << "Sent: RECV -1\\r\\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    while (true) {
        std::string data = read_serial_port(port);
        if (!data.empty()) {
            std::cout << "Raw received: " << data; // デバッグ用に生データも表示

            std::istringstream ss(data);
            std::string line;
            while (std::getline(ss, line)) {
                line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
                line.erase(std::remove(line.begin(), line.end(), '\n'), line.end());
                std::string msg = parse_message(line);
                if (msg.empty()) continue;

                if (msg == "Start" || msg == "Stop" || msg == "True") {
                    write_to_shm(EMER_SHM, msg);
                    std::cout << "[EMERGENCY] " << msg << std::endl;
                } else if (msg.front() == '[' && msg.back() == ']') {
                    write_to_shm(PROP_SHM, msg);
                    std::cout << "[PROP PWM] " << msg << std::endl;
                } else {
                    std::cout << "[UNKNOWN] " << msg << std::endl;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    close(port);
    return 0;
}

