import os
import subprocess
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import pandas as pd
import numpy as np
import time

# Configurations
NUM_GAMES = 100000 
EPOCHS = 5
BATCH_SIZE = 512
LEARNING_RATE = 0.001

class HeirsNNUE(nn.Module):
    def __init__(self):
        super(HeirsNNUE, self).__init__()
        # Input layer: 144 squares * 8 pieces * 2 colors = 2304
        self.fc1 = nn.Linear(2304, 256)
        self.fc2 = nn.Linear(256, 32)
        self.fc3 = nn.Linear(32, 1)
        self.relu = nn.ReLU()
    
    def forward(self, x):
        x = self.relu(self.fc1(x))
        x = self.relu(self.fc2(x))
        x = torch.sigmoid(self.fc3(x))
        return x

class HeirsDataset(Dataset):
    def __init__(self, csv_file):
        print(f"Loading data from {csv_file}...")
        self.data_frame = pd.read_csv(csv_file, header=None)
        print(f"Loaded {len(self.data_frame)} positions.")
        
        self.results = torch.tensor(self.data_frame.iloc[:, 0].values, dtype=torch.float32).view(-1, 1)
        self.evals = torch.tensor(self.data_frame.iloc[:, 1].values, dtype=torch.float32).view(-1, 1)
        
        # We process the 144 categorical features into 2304 binary features
        # The categories are formatted as: color * 10 + piece_type
        # e.g., 0=Empty, 1=WhiteBaby, 12=BlackPrince, etc.
        raw_boards = self.data_frame.iloc[:, 2:].values
        self.boards = torch.zeros(len(self.data_frame), 2304, dtype=torch.float32)
        
        # We map known piece types to a 0-15 index (White pieces 0-7, Black pieces 8-15)
        # Piece types available (1, 2, 3, 4, 5, 6, 7, 8)
        
        # Note: mapping is a bit complex in loop, we'll just vectorize using numpy:
        # 1-8 white pieces map to 0-7.
        # 11-18 black pieces map to 8-15.
        
        print("One-hot encoding dataset... This may take a moment.")
        for row in range(raw_boards.shape[0]):
            for sq in range(144):
                val = raw_boards[row, sq]
                if val == 0:
                    continue # empty
                color = val // 10
                ptype = val % 10
                
                if 1 <= ptype <= 8:
                    channel = color * 8 + (ptype - 1)
                    feature_idx = sq * 16 + channel
                    self.boards[row, feature_idx] = 1.0

    def __len__(self):
        return len(self.data_frame)

    def __getitem__(self, idx):
        # We just use Game Result here as target
        target = self.results[idx]
        return self.boards[idx], target

def generate_data():
    if not os.path.exists("cli/generate_data.exe") and not os.path.exists("cli/generate_data"):
        print("Compiling data generator...")
        compile_cmd = ["g++", "-O3", "-std=c++17", "cli/generate_data.cpp", "src/engine_v1.cpp", "src/board.cpp", "-o", "cli/generate_data"]
        subprocess.run(compile_cmd, check=True)
    
    print("Running self-play generation...")
    exe_name = "cli/generate_data" if os.name != 'nt' else "cli/generate_data.exe"
    subprocess.run([exe_name, str(NUM_GAMES)], check=True)

def train_model():
    dataset = HeirsDataset("selfplay_data.csv")
    dataloader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True)
    
    model = HeirsNNUE()
    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"Training on {device}")
    model.to(device)
    
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)
    
    model.train()
    for epoch in range(EPOCHS):
        running_loss = 0.0
        start = time.time()
        for i, data in enumerate(dataloader):
            inputs, labels = data
            inputs, labels = inputs.to(device), labels.to(device)
            
            optimizer.zero_grad()
            outputs = model(inputs)
            loss = criterion(outputs, labels)
            loss.backward()
            optimizer.step()
            
            running_loss += loss.item()
            
        print(f"Epoch {epoch+1}/{EPOCHS} - Loss: {running_loss/len(dataloader):.4f} - Time: {(time.time()-start):.2f}s")
    
    return model

def export_weights(model, filename="src/nn_weights.h"):
    print(f"Exporting weights to {filename}...")
    with open(filename, "w") as f:
        f.write("#ifndef NN_WEIGHTS_H\n")
        f.write("#define NN_WEIGHTS_H\n\n")
        
        # FC1
        w1 = model.fc1.weight.data.cpu().numpy()
        b1 = model.fc1.bias.data.cpu().numpy()
        
        f.write("const float NN_FC1_WEIGHTS[256][2304] = {\n")
        for i in range(256):
            f.write("    {")
            f.write(", ".join([f"{val:.6f}" for val in w1[i]]))
            f.write("},\n")
        f.write("};\n\n")
        f.write("const float NN_FC1_BIAS[256] = {")
        f.write(", ".join([f"{val:.6f}" for val in b1]))
        f.write("};\n\n")
        
        # FC2
        w2 = model.fc2.weight.data.cpu().numpy()
        b2 = model.fc2.bias.data.cpu().numpy()
        f.write("const float NN_FC2_WEIGHTS[32][256] = {\n")
        for i in range(32):
            f.write("    {")
            f.write(", ".join([f"{val:.6f}" for val in w2[i]]))
            f.write("},\n")
        f.write("};\n\n")
        f.write("const float NN_FC2_BIAS[32] = {")
        f.write(", ".join([f"{val:.6f}" for val in b2]))
        f.write("};\n\n")
        
        # FC3
        w3 = model.fc3.weight.data.cpu().numpy()
        b3 = model.fc3.bias.data.cpu().numpy()
        f.write("const float NN_FC3_WEIGHTS[1][32] = {\n")
        f.write("    {")
        f.write(", ".join([f"{val:.6f}" for val in w3[0]]))
        f.write("}\n};\n\n")
        f.write("const float NN_FC3_BIAS[1] = {")
        f.write(", ".join([f"{val:.6f}" for val in b3]))
        f.write("};\n\n")
        
        f.write("#endif\n")
    print("Export complete. You can now #include \"nn_weights.h\" in your C++ engine.")

if __name__ == "__main__":
    generate_data()
    model = train_model()
    export_weights(model)
