import socket
import json
import time
import csv
import os
import sys

# Cross-platform non-blocking keypress reader
try:
    import msvcrt
    # Windows-specific key reader
    def get_key(timeout=0.1):
        start = time.time()
        while time.time() - start < timeout:
            if msvcrt.kbhit():
                return msvcrt.getch().decode('utf-8', errors='ignore').lower()
            time.sleep(0.01)
        return ''
except ImportError:
    # Unix-specific key reader
    import select
    import termios
    import tty
    def get_key(timeout=0.1):
        settings = termios.tcgetattr(sys.stdin)
        try:
            tty.setraw(sys.stdin.fileno())
            rlist, _, _ = select.select([sys.stdin], [], [], timeout)
            if rlist:
                key = sys.stdin.read(1)
            else:
                key = ''
        finally:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
        return key.lower()

LIMO_IP = "172.16.1.120"  # Change to your LIMO's actual IP
CMD_PORT = 12345

MAX_LINEAR_VEL = 0.2
MAX_STEERING_ANGLE = 0.8
LIN_STEP = 0.05
STEER_STEP = 0.2

def clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))

def main():
    # 1. Setup local storage directories
    log_dir = "training_data"
    csv_path = os.path.join(log_dir, "driving_log.csv")

    # 2. Connect to LIMO TCP Command Server
    cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print(f"Connecting to LIMO commands at {LIMO_IP}:{CMD_PORT}...")
    cmd_sock.connect((LIMO_IP, CMD_PORT))
    # We use a file-like object to easily read until the newline character
    cmd_sock_file = cmd_sock.makefile('r') 

    # 4. Open CSV for logging
    with open(csv_path, 'a', newline='') as f:
        writer = csv.writer(f)
        if os.stat(csv_path).st_size == 0:
            writer.writerow(["timestamp", "linear_vel", "steering_angle", "lidar_data"])

        target_linear = 0.0
        target_steering = 0.0

        print("===============================")
        print("Control LIMO & Collect Data")
        print("===============================")
        print("  W : Increase linear velocity")
        print("  S : Decrease linear velocity")
        print("  A : Turn left (increase steering)")
        print("  D : Turn right (decrease steering)")
        print("  Space : Force stop")
        print("  Q / Ctrl+C : Quit")
        print("===============================")

        try:
            while True:
                start_time = time.time()
                
                # --- A. Get User Input (Teleoperation) ---
                # Using a small timeout here prevents it from severely blocking the data loop
                key = get_key(timeout=0.01)
                
                if key == 'w':
                    target_linear += LIN_STEP
                elif key == 's':
                    target_linear -= LIN_STEP
                elif key == 'a':
                    target_steering += STEER_STEP
                elif key == 'd':
                    target_steering -= STEER_STEP
                elif key == ' ':
                    target_linear = 0.0
                    target_steering = 0.0
                elif key == 'q' or key == '\x03':  # 'q' or Ctrl+C
                    print("\nExiting...")
                    break

                target_linear = clamp(target_linear, -MAX_LINEAR_VEL, MAX_LINEAR_VEL)
                target_steering = clamp(target_steering, -MAX_STEERING_ANGLE, MAX_STEERING_ANGLE)
                
                # --- B. Send Command & Receive LiDAR ---
                cmd_str = f"{target_linear:.4f},{target_steering:.4f}\n"
                cmd_sock.sendall(cmd_str.encode("utf-8"))
                
                # Read the JSON response from the LIMO
                lidar_json = cmd_sock_file.readline().strip()
            
                    
                # --- D. Save Data ---
                timestamp = int(start_time * 1000)
                
                writer.writerow([timestamp, target_linear, target_steering, lidar_json])
                
                # Provide terminal feedback
                sys.stdout.write(f"\rCurrent Target -> Linear: {target_linear:5.2f} m/s, Steering: {target_steering:5.2f} rad  ")
                sys.stdout.flush()
                
                # Throttle to roughly 10Hz to prevent network flooding
                time.sleep(max(0, 0.1 - (time.time() - start_time)))

        except KeyboardInterrupt:
            print("\nInterrupted, stopping robot...")
        except Exception as e:
            print(f"\nError: {e}")
        finally:
            try:
                # Send a final stop command if disconnected
                cmd_sock.sendall("0.0,0.0\n".encode("utf-8"))
            except Exception:
                pass
            cmd_sock.close()
            print("\nSession saved.")

if __name__ == "__main__":
    main()