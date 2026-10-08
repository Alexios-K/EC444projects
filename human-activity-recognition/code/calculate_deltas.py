import json
import re
import os

def process_accelerometer_data(input_file, output_file):
    """Parses the formatted string in arm_accel_data.json to extract X, Y, Z, roll, pitch and calculate deltas."""
    if not os.path.exists(input_file):
        print(f"Skipping {input_file}: File not found.")
        return

    with open(input_file, 'r') as f:
        data = json.load(f)

    # Regex to parse the string: "X: -3.84 \t Y: -8.43 \t Z: 4.28\nroll: -63.12 \t pitch: 22.12 \n"
    pattern = re.compile(r"X:\s*([-\d.]+)\s+Y:\s*([-\d.]+)\s+Z:\s*([-\d.]+)\s+roll:\s*([-\d.]+)\s+pitch:\s*([-\d.]+)")

    prev_vals = None
    for item in data:
        # Extract numerical data from the string
        match = pattern.search(item.get("data", ""))
        if match:
            curr_vals = [float(match.group(i)) for i in range(1, 6)]
            keys = ['x', 'y', 'z', 'roll', 'pitch']
            
            if prev_vals is None:
                # First data point, default deltas to 0.0
                for k in keys:
                    item[f"delta_{k}"] = 0.0
            else:
                # Subtract previous values from current values
                for i, k in enumerate(keys):
                    item[f"delta_{k}"] = round(curr_vals[i] - prev_vals[i], 2)
            
            prev_vals = curr_vals

    with open(output_file, 'w') as f:
        json.dump(data, f, indent=2)
    print(f"Successfully processed accelerometer data to {output_file}")


if __name__ == "__main__":
    base_dir = r"c:\Users\alexk\EC444\01-Team1-Redick-Chen-Kontothanassis\quest-3\code"
    
    accel_in = os.path.join(base_dir, "arm_accel_data.json")
    accel_out = os.path.join(base_dir, "arm_accel_data_with_deltas.json")
    process_accelerometer_data(accel_in, accel_out)