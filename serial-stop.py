import serial
import time
import sys

# --- 設定 ---
# 送信側LoRaモジュールが接続されているシリアルポート
# 環境に合わせて変更してください (例: Windowsでは 'COM3', Linuxでは '/dev/ttyUSB1')
SERIAL_PORT = '/dev/ttyUSB0' 
# ボーレート (C++コードと合わせる)
BAUD_RATE = 115200

# LoRaモジュールの設定
# C++側の受信機(Own=6, Dst=5)に対応させる
OWN_ADDRESS = 5 # 送信元アドレス
DESTINATION_ADDRESS = 6 # 宛先アドレス

def main():
    """
    LoRaモジュールを介してシリアル通信でメッセージを送信するメイン関数
    """
    ser = None # シリアルポートオブジェクトを初期化
    try:
        # 1. シリアルポートを開く
        print(f"シリアルポート {SERIAL_PORT} を開いています (ボーレート: {BAUD_RATE})...")
        ser = serial.Serial(
            SERIAL_PORT,
            BAUD_RATE,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1,  # 読み取りタイムアウトを1秒に設定
            write_timeout=1 # 書き込みタイムアウトを1秒に設定
        )
        print("シリアルポートを正常に開きました。")
        time.sleep(2) # モジュールが起動するのを待つ

        # 2. LoRaモジュールの設定コマンドを送信
        print("\nLoRaモジュールを設定しています...")
        
        # 自局アドレスを設定
        own_cmd = f"Own = {OWN_ADDRESS}\r\n"
        ser.write(own_cmd.encode('utf-8'))
        print(f"送信: {own_cmd.strip()}")
        time.sleep(0.5) # コマンド間の待機
        response = ser.read_until().decode('utf-8', errors='ignore').strip()
        if response:
            print(f"受信: {response}")

        # 宛先アドレスを設定
        dst_cmd = f"Dst = {DESTINATION_ADDRESS}\r\n"
        ser.write(dst_cmd.encode('utf-8'))
        print(f"送信: {dst_cmd.strip()}")
        time.sleep(0.5)
        response = ser.read_until().decode('utf-8', errors='ignore').strip()
        if response:
            print(f"受信: {response}")

        # 設定確認コマンド
        check_cmd = "#?\r\n"
        ser.write(check_cmd.encode('utf-8'))
        print(f"送信: {check_cmd.strip()}")
        time.sleep(0.5)
        # 複数行の応答を読む
        response = ser.read(512).decode('utf-8', errors='ignore').strip()
        if response:
            print(f"受信した設定情報:\n---\n{response}\n---")


        # 3. 'Stop' メッセージを送信
        message_to_send = "Stop"
        send_cmd = f"SEND {message_to_send}\r\n"
        
        print(f"\nメッセージ '{message_to_send}' を送信します...")
        ser.write(send_cmd.encode('utf-8'))
        print(f"送信: {send_cmd.strip()}")
        
        # モジュールからの応答 (例: "OK") を待つ
        time.sleep(1) # 送信完了を待つ
        response = ser.read_until().decode('utf-8', errors='ignore').strip()
        if response:
            print(f"送信後の応答: {response}")
        else:
            print("モジュールからの応答がありませんでした。")

        print("\nメッセージの送信が完了しました。")

    except serial.SerialException as e:
        print(f"エラー: シリアルポート {SERIAL_PORT} を開けませんでした。")
        print(f"詳細: {e}")
        print("ポート名が正しいか、デバイスが接続されているか、他のプログラムが使用中でないか確認してください。")
        sys.exit(1)
    except Exception as e:
        print(f"予期せぬエラーが発生しました: {e}")
    finally:
        # 4. シリアルポートを閉じる
        if ser and ser.is_open:
            ser.close()
            print("\nシリアルポートを閉じました。")

if __name__ == "__main__":
    main()
