import os
import glob
import struct
import pandas as pd
import matplotlib.pyplot as plt

# ==========================================
# 設定
# ==========================================
BIN_PATTERN = "data_*.bin"

FORMAT = "<L8f" # 36 bytes (BMP280対応版)
RECORD_SIZE = struct.calcsize(FORMAT)

# --- 【追加】分割設定 ---
SPLIT_MINUTES = 10 # 何分ごとにファイルを分割するか
SPLIT_ROWS = SPLIT_MINUTES * 60 * 1000 # 分割する行数 (10分 = 600,000行)

def decode_bin(file_path):
    data_list = []
    try:
        with open(file_path, 'rb') as f:
            while chunk := f.read(RECORD_SIZE):
                if len(chunk) == RECORD_SIZE:
                    unpacked = struct.unpack(FORMAT, chunk)
                    data_list.append(unpacked)
    except Exception as e:
        print(f"  [Error] {file_path} の読み込み中にエラー: {e}")
        return None

    if not data_list:
        return None

    columns = ['t_us', 'ax', 'ay', 'az', 'gx', 'gy', 'gz', 'temp', 'pres']
    df = pd.DataFrame(data_list, columns=columns)
    df['t_sec'] = (df['t_us'] - df['t_us'].iloc[0]) / 1000000.0
    return df

def process_file(bin_file):
    base_name = os.path.splitext(bin_file)[0]
    print(f"\nProcessing: {bin_file} ...")
    
    df = decode_bin(bin_file)
    if df is None:
        return False

    total_rows = len(df)
    total_time_min = df['t_sec'].iloc[-1] / 60.0
    print(f"  -> 総データ: {total_rows} 行 (約 {total_time_min:.1f} 分)")

    # データを分割して処理
    num_chunks = (total_rows // SPLIT_ROWS) + 1
    
    for i in range(num_chunks):
        start_idx = i * SPLIT_ROWS
        end_idx = min((i + 1) * SPLIT_ROWS, total_rows)
        
        # チャンク（分割データ）が空なら終了
        if start_idx >= total_rows:
            break
            
        df_chunk = df.iloc[start_idx:end_idx]
        
        # 出力ファイル名 (例: data_000_part1.csv)
        part_suffix = f"_part{i+1}" if num_chunks > 1 else ""
        csv_file = f"{base_name}{part_suffix}.csv"
        png_file = f"{base_name}{part_suffix}.png"
        
        # CSV保存
        df_chunk.to_csv(csv_file, index=False)
        print(f"  [{i+1}/{num_chunks}] CSV出力: {csv_file} ({len(df_chunk)} 行)")
        
        # グラフ描画
        fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(11, 9), sharex=True)
        
        ax1.plot(df_chunk['t_sec'], df_chunk['ax'], label='ax [G]', alpha=0.7, linewidth=1)
        ax1.plot(df_chunk['t_sec'], df_chunk['ay'], label='ay [G]', alpha=0.7, linewidth=1)
        ax1.plot(df_chunk['t_sec'], df_chunk['az'], label='az [G]', alpha=0.7, linewidth=1)
        ax1.set_ylabel('Acceleration [G]')
        ax1.set_title(f"Sensor Data Analysis - {base_name} (Part {i+1})", fontsize=12, fontweight='bold')
        ax1.grid(True, linestyle='--', alpha=0.5); ax1.legend(loc='upper right')

        ax2.plot(df_chunk['t_sec'], df_chunk['gx'], label='gx [deg/s]', alpha=0.7, linewidth=1)
        ax2.plot(df_chunk['t_sec'], df_chunk['gy'], label='gy [deg/s]', alpha=0.7, linewidth=1)
        ax2.plot(df_chunk['t_sec'], df_chunk['gz'], label='gz [deg/s]', alpha=0.7, linewidth=1)
        ax2.set_ylabel('Gyro [deg/s]')
        ax2.grid(True, linestyle='--', alpha=0.5); ax2.legend(loc='upper right')

        ax3.plot(df_chunk['t_sec'], df_chunk['pres'], label='Pressure [hPa]', color='purple', alpha=0.8, linewidth=1.2)
        ax3.set_ylabel('Pressure [hPa]')
        ax3.set_xlabel('Time [sec]')
        ax3.grid(True, linestyle='--', alpha=0.5); ax3.legend(loc='upper right')

        plt.tight_layout()
        plt.savefig(png_file, dpi=300)
        plt.close(fig)
        
    return True

def main():
    bin_files = sorted(glob.glob(BIN_PATTERN))
    if not bin_files:
        print("対象ファイルが見つかりません。")
        return
        
    print(f"=== {len(bin_files)} 個のファイルを検出 ===")
    
    success_count = 0
    for bin_file in bin_files:
        if process_file(bin_file):
            success_count += 1
            
    print(f"\n完了: {success_count} / {len(bin_files)} 個を処理しました。")

if __name__ == "__main__":
    main()