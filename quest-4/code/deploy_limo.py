import torch
import torch.nn as nn
import socket
import json
import numpy as np
import time
import sys
import select

# Try importing Windows-specific or Linux-specific terminal libraries for get_key
if sys.platform == 'win32':
    import msvcrt
else:
    import tty
    import termios

# --- 1. Configuration ---
LIMO_IP = "172.16.1.120"
CMD_PORT = 12345
MODEL_PATH = "limo_autopilot.pt"
MAX_DISTANCE = 8000.0
DRIVE_SPEED = 0.15

# Manual Override Constraints
LIN_STEP = 0.05
STEER_STEP = 0.1
MAX_LINEAR_VEL = 0.2
MAX_STEERING_ANGLE = 0.5

# --- 2. The Neural Network Architecture ---
class LimoBrain(nn.Module):
    def __init__(self):
        super(LimoBrain, self).__init__()
        self.conv_layers = nn.Sequential(
            nn.Conv1d(in_channels=1, out_channels=16, kernel_size=5, stride=2, padding=2),
            nn.ReLU(),
            nn.Conv1d(in_channels=16, out_channels=32, kernel_size=5, stride=2, padding=2),
            nn.ReLU(),
            nn.Conv1d(in_channels=32, out_channels=64, kernel_size=3, stride=2, padding=1),
            nn.ReLU()
        )
        self.pool = nn.AdaptiveAvgPool1d(10)
        self.fc_layers = nn.Sequential(
            nn.Flatten(),
            nn.Linear(64 * 10, 128),
            nn.ReLU(),
            nn.Linear(128, 1)
        )

    def forward(self, x):
        x = self.conv_layers(x)
        x = self.pool(x)
        x = self.fc_layers(x)
        return x

# --- 3. Preprocessing Function ---
def preprocess_lidar(lidar_json_str):
    try:
        raw_scan = json.loads(lidar_json_str)
    except json.JSONDecodeError:
        raw_scan = []
        
    scan_array = np.full(360, MAX_DISTANCE, dtype=np.float32)
    
    for point in raw_scan:
        angle, distance = point[0], point[1]
        if distance > 0:
            idx_angle = int(round(angle)) % 360
            if distance < scan_array[idx_angle]:
                scan_array[idx_angle] = distance
                
    scan_array = scan_array / MAX_DISTANCE
    return torch.tensor(scan_array, dtype=torch.float32).unsqueeze(0).unsqueeze(0)

# --- 4. Non-Blocking Keyboard Input ---
def get_key(timeout=0.01):
    """Cross-platform non-blocking keystroke catcher."""
    if sys.platform == 'win32':
        start = time.time()
        while True:
            if msvcrt.kbhit():
                return msvcrt.getwch().lower()
            if time.time() - start > timeout:
                return ''
    else:
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            rlist, _, _ = select.select([sys.stdin], [], [], timeout)
            if rlist:
                key = sys.stdin.read(1).lower()
            else:
                key = ''
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return key

# --- 5. The Autonomous/Manual Control Loop ---
def main():
    print("Loading AI Brain...")
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = LimoBrain().to(device)
    model.load_state_dict(torch.load(MODEL_PATH, map_location=device))
    model.eval() 
    print("Brain loaded successfully.")

    cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    cmd_sock.settimeout(2.0)
    
    print(f"Connecting to LIMO at {LIMO_IP}:{CMD_PORT}...")
    cmd_sock.connect((LIMO_IP, CMD_PORT))
    cmd_sock_file = cmd_sock.makefile('r')

    print("\n==========================================")
    print("--- DEPLOYMENT READY ---")
    print(" Current Mode: AUTONOMOUS")
    print(" [Space] : Halt AI and switch to MANUAL (WASD)")
    print(" [Q]     : Switch back to AUTONOMOUS")
    print(" [Esc]   : Quit program entirely")
    print("==========================================\n")
    
    mode = "AUTONOMOUS"
    target_linear = 0.0
    target_steering = 0.0
    
    try:
        # Kickoff with a safe command
        cmd_sock.sendall("0.0,0.0\n".encode("utf-8"))
        
        while True:
            start_time = time.time()
            
            # --- 1. Catch Keyboard State ---
            key = get_key(timeout=0.01)
            
            if key == ' ':
                if mode != "MANUAL":
                    mode = "MANUAL"
                    target_linear = 0.0
                    target_steering = 0.0
                    print("\n>> OVERRIDE ENGAGED: MANUAL MODE (WASD to drive, Q for Auto)")
            elif key == 'q':
                if mode != "AUTONOMOUS":
                    mode = "AUTONOMOUS"
                    print("\n>> AI ENGAGED: AUTONOMOUS MODE (Space to Override)")
            elif key == '\x1b' or key == '\x03':  # Esc or Ctrl+C
                print("\nExiting...")
                break

            # --- 2. Network Read (Clear the buffer to prevent lag) ---
            lidar_json = ""
            try:
                lidar_json = cmd_sock_file.readline().strip()
            except socket.timeout:
                # If network drops, ensure we don't send phantom manual commands
                cmd_sock.sendall("0.0,0.0\n".encode("utf-8"))
                continue

            # --- 3. Execute Mode ---
            if mode == "MANUAL":
                if key == 'w': target_linear += LIN_STEP
                elif key == 's': target_linear -= LIN_STEP
                elif key == 'a': target_steering += STEER_STEP
                elif key == 'd': target_steering -= STEER_STEP
                
                # Clamp inputs
                target_linear = max(-MAX_LINEAR_VEL, min(MAX_LINEAR_VEL, target_linear))
                target_steering = max(-MAX_STEERING_ANGLE, min(MAX_STEERING_ANGLE, target_steering))
                
                # Send manual command
                cmd_str = f"{target_linear:.4f},{target_steering:.4f}\n"
                cmd_sock.sendall(cmd_str.encode("utf-8"))
                
                print(f"MANUAL | Speed: {target_linear:.2f} | Steer: {target_steering:.2f}", end="\r")

            elif mode == "AUTONOMOUS":
                if not lidar_json:
                    continue
                    
                # Process Data through Model
                features = preprocess_lidar(lidar_json).to(device)
                
                with torch.no_grad(): 
                    prediction = model(features)
                    steering_angle = prediction.item()
                    
                # Send AI command
                cmd_str = f"{DRIVE_SPEED:.4f},{steering_angle:.4f}\n"
                cmd_sock.sendall(cmd_str.encode("utf-8"))
                
                print(f"AUTO   | Speed: {DRIVE_SPEED:.2f} | AI Steer: {steering_angle:.3f}", end="\r")
            
            # --- 4. Throttle Loop (maintain ~10Hz) ---
            elapsed = time.time() - start_time
            if elapsed < 0.1:
                time.sleep(0.1 - elapsed)

    finally:
        try:
            cmd_sock.sendall("0.0,0.0\n".encode("utf-8"))
        except:
            pass 
        cmd_sock.close()
        print("\nRobot stopped safely.")

if __name__ == "__main__":
    main()