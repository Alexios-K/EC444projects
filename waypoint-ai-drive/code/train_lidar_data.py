import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import pandas as pd
import numpy as np
import json
import os

# --- 1. Parameters ---
CSV_FILE = "training_data/driving_log.csv"
MAX_DISTANCE = 8000.0  # Maximum reliable distance of your LiDAR in mm
BATCH_SIZE = 32
EPOCHS = 20
LEARNING_RATE = 0.001

# --- 2. The Dataset & Preprocessing ---
class LimoDataset(Dataset):
    def __init__(self, csv_file):
        self.data = pd.read_csv(csv_file)
        
    def __len__(self):
        return len(self.data)
        
    def __getitem__(self, idx):
        row = self.data.iloc[idx]
        
        # Parse the JSON string of LiDAR points
        try:
            raw_scan = json.loads(row['lidar_data'])
        except json.JSONDecodeError:
            raw_scan = []
            
        # Initialize a 360-element array filled with the max distance (open space)
        scan_array = np.full(360, MAX_DISTANCE, dtype=np.float32)
        
        # Bin the raw angles into the 360-degree array
        for point in raw_scan:
            angle, distance = point[0], point[1]
            if distance > 0: # Ignore 0.0 errors from the sensor
                idx_angle = int(round(angle)) % 360
                # If multiple points hit the same degree, keep the closest one (the actual wall)
                if distance < scan_array[idx_angle]:
                    scan_array[idx_angle] = distance
                    
        # Normalize the distances so they are between 0.0 and 1.0 (Neural Nets love this)
        scan_array = scan_array / MAX_DISTANCE
        
        # PyTorch Conv1D expects shape: (Channels, Length). We have 1 channel, 360 length.
        features = torch.tensor(scan_array, dtype=torch.float32).unsqueeze(0)
        
        # Extract the steering angle (our label)
        label = torch.tensor([row['steering_angle']], dtype=torch.float32)
        
        return features, label

# --- 3. The Neural Network (Conv1D) ---
class LimoBrain(nn.Module):
    def __init__(self):
        super(LimoBrain, self).__init__()
        
        # 1D Convolutions to detect "shapes" and "walls" in the LiDAR array
        self.conv_layers = nn.Sequential(
            nn.Conv1d(in_channels=1, out_channels=16, kernel_size=5, stride=2, padding=2),
            nn.ReLU(),
            nn.Conv1d(in_channels=16, out_channels=32, kernel_size=5, stride=2, padding=2),
            nn.ReLU(),
            nn.Conv1d(in_channels=32, out_channels=64, kernel_size=3, stride=2, padding=1),
            nn.ReLU()
        )
        
        # Compress the output to a fixed size before the Linear layers
        self.pool = nn.AdaptiveAvgPool1d(10)
        
        self.fc_layers = nn.Sequential(
            nn.Flatten(),
            nn.Linear(64 * 10, 128),
            nn.ReLU(),
            nn.Linear(128, 1) # Output a single float: steering_angle
        )

    def forward(self, x):
        x = self.conv_layers(x)
        x = self.pool(x)
        x = self.fc_layers(x)
        return x

# --- 4. The Training Loop ---
def main():
    print("Loading dataset...")
    dataset = LimoDataset(CSV_FILE)
    dataloader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)
    
    # Check for GPU (CUDA)
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on device: {device}")
    
    model = LimoBrain().to(device)
    
    # MSELoss is standard for regression tasks (predicting an exact number)
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)
    
    print("Starting training...")
    for epoch in range(EPOCHS):
        running_loss = 0.0
        for features, labels in dataloader:
            features, labels = features.to(device), labels.to(device)
            
            # 1. Zero the gradients
            optimizer.zero_grad()
            
            # 2. Forward pass (predict the steering angle)
            outputs = model(features)
            
            # 3. Calculate how wrong the prediction was
            loss = criterion(outputs, labels)
            
            # 4. Backward pass (learn from the mistake)
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item()
            
        avg_loss = running_loss / len(dataloader)
        print(f"Epoch [{epoch+1}/{EPOCHS}] - Loss: {avg_loss:.4f}")
        
    # --- 5. Save the Model ---
    model_path = "limo_autopilot.pt"
    torch.save(model.state_dict(), model_path)
    print(f"\nTraining complete! Model weights saved to {model_path}")

if __name__ == "__main__":
    main()