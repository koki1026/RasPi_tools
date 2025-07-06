#include <iostream>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>
#include <errno.h>
#include <atomic>
#include <thread>

// --- 関数プロトタイプ（宣言） ---
int open_serial_port(const char* device, speed_t baud_rate);
void init_lora_module(int serial_port);
void write_raw_serial(int serial_port, const std::string& raw_command);
void write_serial_port(int serial_port, const std::string& data);
std::string read_serial_port(int serial_port);
std::string read_serial_port_multi(int serial_port, int max_attempts = 10, int wait_ms = 100);
void init_lora_module(int serial_port);

std::atomic<bool> heartbeat_running(false);

// シリアルポートを開く
int open_serial_port(const char* device, speed_t baud_rate) {
    int serial_port = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (serial_port < 0) {
        std::cerr << "Error opening serial port: " << strerror(errno) << std::endl;
        return -1;
    }

    fcntl(serial_port, F_SETFL, 0); // blockingモードへ

    struct termios tty;
    memset(&tty, 0, sizeof tty);

    if (tcgetattr(serial_port, &tty) != 0) {
        std::cerr << "Error from tcgetattr: " << strerror(errno) << std::endl;
        close(serial_port);
        return -1;
    }

    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);

    tty.c_cflag |= (CLOCAL | CREAD); // Enable receiver
    tty.c_cflag &= ~PARENB; // No parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;     // 8 data bits
    tty.c_cflag &= ~CRTSCTS; // No flow control

    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // Raw input
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // No software flow control
    tty.c_oflag &= ~OPOST; // Raw output

    // 1秒待つ・最低1バイト
    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;

    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        std::cerr << "Error from tcsetattr: " << strerror(errno) << std::endl;
        close(serial_port);
        return -1;
    }

    return serial_port;
}

// LoRaモジュールの初期化
void init_lora_module(int serial_port) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    write_raw_serial(serial_port, "Own = 5");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    write_raw_serial(serial_port, "Dst = 6");
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    write_raw_serial(serial_port, "#?");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    
    std::string response = read_serial_port_multi(serial_port, 10, 100);
    if (!response.empty()) {
        std::cout << "Initial LoRa Response:\n" << response << std::endl;
    }
}
// SENDなしでそのまま送る関数
void write_raw_serial(int serial_port, const std::string& raw_command) {
    std::string command_with_crlf = raw_command + "\r\n";
    ssize_t bytes_written = write(serial_port, command_with_crlf.c_str(), command_with_crlf.length());
    if (bytes_written < 0) {
        std::cerr << "Error writing raw command: " << strerror(errno) << std::endl;
    } else {
        std::cout << "Sent (raw): " << command_with_crlf;
    }
}

// シリアルポートに書き込み
void write_serial_port(int serial_port, const std::string& data) {
    std::string full_command = "SEND \"" + data + "\"\r\n";
    ssize_t bytes_written = write(serial_port, full_command.c_str(), full_command.length());
    if (bytes_written < 0) {
        std::cerr << "Error writing: " << strerror(errno) << std::endl;
    }
    else if (data == "True") {
        // Trueだった場合はcoutなしで終了
    }
    // それ以外のコマンドは出力する 
    else {
        std::cout << "Sent: " << full_command;
    }
}

void send_heartbeat_loop(int serial_port) {
    while (heartbeat_running.load()) {
        write_serial_port(serial_port, "True");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

// 応答読み取り（任意）
std::string read_serial_port(int serial_port) {
    char buf[256] = {0};
    int n = read(serial_port, buf, sizeof(buf));
    if (n > 0) {
        return std::string(buf, n);
    }
    return "";
}

std::string read_serial_port_multi(int serial_port, int max_attempts, int wait_ms) {
    std::string total_data;
    char buf[256];

    for (int i = 0; i < max_attempts; ++i) {
        memset(buf, 0, sizeof(buf));
        ssize_t n = read(serial_port, buf, sizeof(buf) - 1);
        if (n > 0) {
            total_data += std::string(buf, n);
        } else if (n == 0) {
            break; // No more data
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms));
    }

    return total_data;
}

int main() {
    const char* serial_device = "/dev/ttyUSB0";
    speed_t baud = B115200;

    int serial = open_serial_port(serial_device, baud);
    if (serial < 0) return 1;

    // ← ここで初期設定を呼び出す
    init_lora_module(serial);

    std::cout << "Serial port opened. Type Start / Stop . Type 'exit' to quit.\n";
    std::cout << "Type 'start_heartbeat' to begin sending heartbeat messages.\n";
    std::cout << "Type 'stop_heartbeat' to stop sending heartbeat messages.\n";

    std::string input;

    std::thread heartbeat_thread;

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        if (input == "exit") break;
        else if (input == "start_heartbeat") {
            if (!heartbeat_running) {
                heartbeat_running = true;
                heartbeat_thread = std::thread(send_heartbeat_loop, serial);
                std::cout << "Heartbeat started.\n";
            } else {
                std::cout << "Heartbeat is already running.\n";
            }
            continue;
        } else if (input == "stop_heartbeat") {
            if (heartbeat_running) {
                heartbeat_running = false;
                if (heartbeat_thread.joinable()) {
                    heartbeat_thread.join();
                }
                std::cout << "Heartbeat stopped.\n";
            } else {
                std::cout << "Heartbeat was not running.\n";
            }
            continue;
        }
        if (input.empty()) continue;

        // 通常のコマンド（Start, Stop, Trueなど）
        if (!input.empty()) {
            write_serial_port(serial, input);
        }
    }
    // ハートビートスレッドが動いている場合は停止
    if (heartbeat_running) {
        heartbeat_running = false;
        if (heartbeat_thread.joinable()) {
            heartbeat_thread.join();
        }
        std::cout << "Heartbeat stopped.\n";
    }
    // シリアルポートを閉じる
    close(serial);
    std::cout << "Serial port closed. Exiting.\n";
    return 0;
}
