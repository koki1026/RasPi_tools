#include <iostream>
#include <string>
#include <thread>      // For std::this_thread::sleep_for
#include <chrono>      // For std::chrono::milliseconds
#include <regex>       // For regular expressions to parse received data
#include <algorithm>   // For std::remove

// GPIO制御用 (pigpio)
#include <pigpio.h>

// シリアル通信用
#include <fcntl.h>     // File control options
#include <termios.h>   // POSIX terminal control definitions
#include <unistd.h>    // UNIX standard function definitions
#include <cstring>     // For memset, memcpy, strerror
#include <errno.h>     // For errno

// --- シリアルポート制御関数群 ---
// これらの関数を前回の説明からコピーして追加します

// シリアルポートを開く
int open_serial_port(const char* device, speed_t baud_rate) {
    int serial_port = open(device, O_RDWR | O_NOCTTY | O_NDELAY); // O_NDELAY: Non-blocking open
    if (serial_port < 0) {
        std::cerr << "Error " << errno << " from open: " << strerror(errno) << std::endl;
        return -1;
    }

    // Restore blocking mode
    fcntl(serial_port, F_SETFL, 0);

    struct termios tty;
    memset(&tty, 0, sizeof(tty)); // Clear struct

    // Get current attributes
    if (tcgetattr(serial_port, &tty) != 0) {
        std::cerr << "Error " << errno << " from tcgetattr: " << strerror(errno) << std::endl;
        close(serial_port); // Close before returning error
        return -1;
    }

    // Set baud rates
    cfsetospeed(&tty, baud_rate);
    cfsetispeed(&tty, baud_rate);

    // Control options
    tty.c_cflag &= ~PARENB; // No parity
    tty.c_cflag &= ~CSTOPB; // 1 stop bit
    tty.c_cflag &= ~CSIZE;  // Clear all size bits
    tty.c_cflag |= CS8;     // 8 data bits
    tty.c_cflag &= ~CRTSCTS; // Disable hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines

    // Local options
    tty.c_lflag &= ~ICANON; // Disable canonical mode (raw input)
    tty.c_lflag &= ~ECHO;   // Disable echo
    tty.c_lflag &= ~ECHOE;  // Disable erasure
    tty.c_lflag &= ~ECHONL; // Disable new-line echo
    tty.c_lflag &= ~ISIG;   // Disable interpretation of INTR, QUIT and SUSP

    // Input options
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow control
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable special handling of received bytes

    // Output options
    tty.c_oflag &= ~OPOST; // Prevent special interpretation of output bytes (e.g. newline chars)
    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed

    // Blocking read settings
    tty.c_cc[VTIME] = 10; // Wait for up to 1 second (10 * 0.1s)
    tty.c_cc[VMIN] = 0;   // Don't wait for any specific number of bytes

    // Apply changes
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        std::cerr << "Error " << errno << " from tcsetattr: " << strerror(errno) << std::endl;
        close(serial_port); // Close before returning error
        return -1;
    }

    return serial_port;
}

// シリアルポートから読み込み
std::string read_serial_port(int serial_port) {
    char read_buf[256];
    // Clear the buffer to avoid stale data
    memset(read_buf, 0, sizeof(read_buf));
    ssize_t num_bytes = read(serial_port, &read_buf, sizeof(read_buf) - 1);
    if (num_bytes < 0) {
        // If errno is EAGAIN or EWOULDBLOCK, it means no data is available right now (non-blocking read)
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ""; // No data available
        }
        std::cerr << "Error reading: " << strerror(errno) << std::endl;
        return "";
    }
    if (num_bytes == 0) {
        return ""; // No data
    }
    read_buf[num_bytes] = '\0'; // Null-terminate the string
    return std::string(read_buf);
}

// シリアルポートに書き込み
void write_serial_port(int serial_port, const std::string& data) {
    ssize_t bytes_written = write(serial_port, data.c_str(), data.length());
    if (bytes_written < 0) {
        std::cerr << "Error writing: " << strerror(errno) << std::endl;
    } else if (bytes_written != static_cast<ssize_t>(data.length())) {
        std::cerr << "Warning: Not all data was written to serial port." << std::endl;
    }
}

// シリアルポートを閉じる
void close_serial_port(int serial_port) {
    if (serial_port >= 0) {
        close(serial_port);
    }
}

// --- メインプログラム ---

#define RELAY_PIN 17 // GPIO 17 をリレー制御に使用

// 受信データ解析関数
std::string parse_received_message(const std::string& raw_data) {
    // 例: "@-18,6,Hello" から "Hello" を抽出
    // 正規表現を使って、3番目のカンマ以降の文字列を取得
    std::regex re("@-?\\d+,-?\\d+,(.*)");
    std::smatch matches;
    if (std::regex_search(raw_data, matches, re) && matches.size() > 1) {
        return matches[1].str();
    }
    return ""; // 解析失敗
}

int main() {
    // 1. pigpioの初期化
    if (gpioInitialise() < 0) {
        std::cerr << "pigpio initialization failed." << std::endl;
        return 1;
    }

    // 2. GPIOピンの設定
    gpioSetMode(RELAY_PIN, PI_OUTPUT);
    gpioWrite(RELAY_PIN, PI_LOW); // 初期状態はLow（リレーOFF）

    // 3. シリアルポートの初期化 (LoRaモジュール接続用)
    // /dev/ttyUSB0 はRaspberry Piの場合も同じです
    const char* lora_serial_device = "/dev/ttyUSB0"; // あなたの環境に合わせて変更
    // ボーレートはpicocomで接続できた115200bpsに合わせています。LRA1の実際のボーレートを確認してください。
    speed_t lora_baud_rate = B115200; // LRA1のボーレートに合わせる

    int lora_port = open_serial_port(lora_serial_device, lora_baud_rate);
    if (lora_port < 0) {
        std::cerr << "Failed to open LoRa serial port." << std::endl;
        gpioTerminate();
        return 1;
    }
    std::cout << "LoRa serial port opened successfully." << std::endl;

    // 4. LRA1モジュールを受信モードに設定
    // RECV -1 コマンドを送信して、常に受信待ちにする
    // ATコマンドの終端は \r\n (CRLF) であることを確認してください
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    write_serial_port(lora_port, "Own = 6\r\n");
    std::cout << "Sent: Own = 6\\r\\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    write_serial_port(lora_port, "Dst = 5\r\n");
    std::cout << "Sent: Dst = 5\\r\\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    //確認コマンド
    write_serial_port(lora_port, "#?\r\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    write_serial_port(lora_port, "RECV -1\r\n");
    std::cout << "Sent: RECV -1\\r\\n" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // 5. メインループ：LoRaからの信号受信とGPIO制御
    std::cout << "Waiting for LoRa commands ('Start' or 'Stop')..." << std::endl;
    while (true) {
        std::string raw_received_data = read_serial_port(lora_port);
        if (!raw_received_data.empty()) {
            std::cout << "Raw received: " << raw_received_data; // デバッグ用に生データも表示

            // 受信データの整形 (改行コードや不要な文字を除去)
            raw_received_data.erase(std::remove(raw_received_data.begin(), raw_received_data.end(), '\r'), raw_received_data.end());
            raw_received_data.erase(std::remove(raw_received_data.begin(), raw_received_data.end(), '\n'), raw_received_data.end());

            std::string actual_message = parse_received_message(raw_received_data);

            if (!actual_message.empty()) {
                std::cout << "Parsed message: " << actual_message << std::endl;

                if (actual_message == "Start") {
                    gpioWrite(RELAY_PIN, PI_HIGH); // リレーON (プロペラ通電)
                    std::cout << "RELAY: HIGH (Propeller ON)" << std::endl;
                } else if (actual_message == "Stop") {
                    gpioWrite(RELAY_PIN, PI_LOW); // リレーOFF (プロペラ停止)
                    std::cout << "RELAY: LOW (Propeller OFF)" << std::endl;
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // ポーリング間隔を調整
    }

    // 6. 終了処理 (通常は無限ループのため到達しないが、エラーハンドリングのために記述)
    // プログラムが何らかの理由で終了した場合に備えてクリーンアップ
    close_serial_port(lora_port);
    gpioTerminate();
    std::cout << "Program terminated. GPIO cleaned up." << std::endl;

    return 0;
}
