import json
import re
import os

def process_accelerometer_data(input_file, output_file):
    """Parses accelerometer data and replaces 'data' with only delta values."""
    if not os.path.exists(input_file):
        print(f"Skipping {input_file}: File not found.")
        return

    with open(input_file, 'r') as f:
        data = json.load(f)

    pattern = re.compile(
        r"X:\s*([-\d.]+)\s+Y:\s*([-\d.]+)\s+Z:\s*([-\d.]+)\s+roll:\s*([-\d.]+)\s+pitch:\s*([-\d.]+)"
    )

    prev_vals = None

    for item in data:
        match = pattern.search(item.get("data", ""))

        if match:
            curr_vals = [float(match.group(i)) for i in range(1, 6)]

            # Compute deltas
            if prev_vals is None:
                deltas = [0.0] * 5
            else:
                deltas = [round(curr_vals[i] - prev_vals[i], 2) for i in range(5)]

            prev_vals = curr_vals

            # Replace data field with ONLY delta values (tab-separated)
            item["data"] = (
                f"X: {deltas[0]} \t Y: {deltas[1]} \t Z: {deltas[2]} "
                f"\t roll: {deltas[3]} \t pitch: {deltas[4]}"
            )

            # Remove old delta keys if they exist
            for k in ['x', 'y', 'z', 'roll', 'pitch']:
                item.pop(f"delta_{k}", None)

    with open(output_file, 'w') as f:
        json.dump(data, f, indent=2)

    print(f"Successfully processed accelerometer data to {output_file}")


if __name__ == "__main__":
    base_dir = r"/Users/home/EC444/accelerometer_classifier/"
    
    accel_in = os.path.join(base_dir, "arm_accel_data.json")
    accel_out = os.path.join(base_dir, "arm_accel_data_with_deltas.json")
    process_accelerometer_data(accel_in, accel_out)