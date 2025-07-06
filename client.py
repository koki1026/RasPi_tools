import socket
import time
import subprocess
import time

HOST = '192.168.2.2'      # JetsonのIPアドレス (サーバーのIPアドレス)
PORT = 65432            # 使用するポート番号
MAX_RETRIES = 5         # 接続試行の最大回数
RETRY_DELAY = 3         # 再試行までの待機時間（秒）
CYCLE_DELAY = 10        # 次のサイクルまでの待機時間（秒）

# --- 接続試行フェーズ ---

while True:
    s = None

    for attempt in range(MAX_RETRIES):
        try:

            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

            s.settimeout(5)
            print(f"Attempting to connect to {HOST}:{PORT} (Attempt {attempt + 1}/{MAX_RETRIES})...")
            s.connect((HOST, PORT))
            print(f"Successfully connected to {HOST}:{PORT}")

            break

        except (ConnectionRefusedError, socket.timeout, OSError) as e:
            print(f"Connection attempt failed: {e}")
            if s:
                s.close()
            s = None

            if attempt < MAX_RETRIES - 1:
                print(f"Retrying in {RETRY_DELAY} seconds...")
                time.sleep(RETRY_DELAY)

    # --- 通信フェーズ ---

    if s:
        try:
            s.settimeout(10)

            print("Waiting to receive data from server...")
            data = s.recv(1024)
            print(f"Received from server: {data.decode()}")

            s.sendall(b"Hello from Raspberry Pi!")
            print("Communication successful.")
            #
            # ここに接続成功を確認したときのコードを書く
            #

        except socket.timeout:
            print("Data reception timed out. Server did not send data within the time limit.")
        except Exception as e:
            print(f"An error occurred during communication: {e}")
        finally:
            print("Closing the socket.")
            s.close()

        print(f"Waiting for {CYCLE_DELAY} seconds before starting the next connection cycle...")
        time.sleep(CYCLE_DELAY)

    else: # 接続フェーズで5回すべて失敗した場合
        print(f"Failed to connect to the server after {MAX_RETRIES} attempts.")
            #
            # ここに接続失敗を確認したときのコードを書く
            #
        try:
            # python3インタープリタを使って 'serial-stop.py' を実行する
            # check=True: スクリプトがエラーで終了した場合に例外を発生させる
            # capture_output=True: 標準出力と標準エラー出力をキャプチャする
            # text=True: 出力を文字列として扱う
            result = subprocess.run(
                ['python3', 'serial-stop.py'], 
                check=True, 
                capture_output=True, 
                text=True
            )
            print("--- 'serial-stop.py' execution successful ---")
            print("Output from script:")
            print(result.stdout) # 実行したスクリプトの出力を表示
            print("--- End of script output ---")

        except FileNotFoundError:
            print("Error: 'serial-stop.py' not found. Make sure it is in the same directory as client.py.")
        except subprocess.CalledProcessError as e:
            # 実行したスクリプトがエラーを返した場合
            print(f"Error executing 'serial-stop.py'. It returned a non-zero exit code {e.returncode}.")
            print("--- STDOUT ---")
            print(e.stdout)
            print("--- STDERR ---")
            print(e.stderr) # エラー出力を表示
        except Exception as e:
            print(f"An unexpected error occurred while trying to run 'serial-stop.py': {e}")

        print(f"Waiting for {RETRY_DELAY} seconds before starting a new connection cycle.")
        time.sleep(RETRY_DELAY)


