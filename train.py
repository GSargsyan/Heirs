import os
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
import pandas as pd
import numpy as np
import time

# Configurations
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
        print(f"Loading data from {csv_file} in chunks to save RAM...")
        # Use minimal types to save RAM: float32 for outputs, uint8 for board pieces
        dtypes = {0: np.float32, 1: np.float32}
        for i in range(2, 146):
            dtypes[i] = np.uint8
            
        chunk_iter = pd.read_csv(csv_file, header=None, dtype=dtypes, chunksize=100000)
        
        results_list = []
        boards_list = []
        
        for i, chunk in enumerate(chunk_iter):
            results_list.append(torch.tensor(chunk.iloc[:, 0].values, dtype=torch.float32))
            boards_list.append(torch.tensor(chunk.iloc[:, 2:].values, dtype=torch.uint8))
            if (i+1) % 10 == 0:
                print(f"Loaded {(i+1) * 100000} positions...")
            
        self.results = torch.cat(results_list, dim=0).view(-1, 1)
        self.raw_boards = torch.cat(boards_list, dim=0)
        
        print(f"Successfully loaded {len(self.results)} positions into memory.")

    def __len__(self):
        return len(self.raw_boards)

    def __getitem__(self, idx):
        target = self.results[idx]
        raw_board = self.raw_boards[idx].long()
        
        board_tensor = torch.zeros(2304, dtype=torch.float32)
        
        mask = raw_board != 0
        valid_boards = raw_board[mask]
        valid_sqs = torch.nonzero(mask).squeeze(1)
        
        colors = valid_boards // 10
        ptypes = valid_boards % 10
        
        channels = colors * 8 + (ptypes - 1)
        
        valid_ptypes_mask = (ptypes >= 1) & (ptypes <= 8)
        
        feature_idx = valid_sqs[valid_ptypes_mask] * 16 + channels[valid_ptypes_mask]
        board_tensor[feature_idx] = 1.0
        
        return board_tensor, target

def train_model():
    dataset = HeirsDataset("selfplay_data.csv")
    
    # We use multiple workers to speed up the __getitem__ preprocessing during training
    dataloader = DataLoader(dataset, batch_size=BATCH_SIZE, shuffle=True, num_workers=2, pin_memory=True)
    
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
            
            if i % 100 == 99:    # Print every 100 batches
                print(f"Epoch {epoch+1} - Batch {i+1} - Avg Loss: {running_loss / 100:.4f}")
                running_loss = 0.0
                
        print(f"Epoch {epoch+1}/{EPOCHS} completed - Time: {(time.time()-start):.2f}s")
    
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
    model = train_model()
    export_weights(model)
