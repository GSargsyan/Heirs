import torch
import torch.nn as nn
import torch.nn.functional as F

class HeirsNNet(nn.Module):
    def __init__(self, game, args):
        super(HeirsNNet, self).__init__()
        self.board_x, self.board_y = game.getBoardSize() # 13x12
        self.action_size = game.getActionSize()
        self.args = args

        # CHANGE 1: 17 channels (16 for piece types, 1 for the scaled clock)
        self.conv1 = nn.Conv2d(17, args.num_channels, 3, stride=1, padding=1)
        self.conv2 = nn.Conv2d(args.num_channels, args.num_channels, 3, stride=1, padding=1)
        self.conv3 = nn.Conv2d(args.num_channels, args.num_channels, 3, stride=1, padding=1)
        self.conv4 = nn.Conv2d(args.num_channels, args.num_channels, 3, stride=1, padding=1)

        self.bn1 = nn.BatchNorm2d(args.num_channels)
        self.bn2 = nn.BatchNorm2d(args.num_channels)
        self.bn3 = nn.BatchNorm2d(args.num_channels)
        self.bn4 = nn.BatchNorm2d(args.num_channels)

        # CHANGE 2: The spatial size is 12x12, NOT 13x12
        self.fc1 = nn.Linear(args.num_channels * 12 * 12, 1024)
        self.fc_bn1 = nn.BatchNorm1d(1024)
        self.fc2 = nn.Linear(1024, 512)
        self.fc_bn2 = nn.BatchNorm1d(512)
        self.fc3 = nn.Linear(512, self.action_size)
        self.fc4 = nn.Linear(512, 1)

    def forward(self, s):
        # s: batch_size x 13 x 12
        s_board = s[:, :12, :] # Extract the 12x12 physical board
        s_clock = s[:, 12, 0:1].view(-1, 1, 1, 1) # Extract clock scalar
        
        # CHANGE 3: One-hot encode the 16 piece types
        channels =[]
        for p in range(1, 9):       # White pieces (1 to 8)
            channels.append((s_board == p).float().unsqueeze(1))
        for p in range(-8, 0):      # Black pieces (-8 to -1)
            channels.append((s_board == p).float().unsqueeze(1))
            
        # Normalize the 50-move clock (0 to 1) and expand to a 12x12 plane
        clock_channel = (s_clock / 100.0).expand(-1, -1, 12, 12)
        
        # Combine into a (batch_size, 17, 12, 12) tensor
        s_transformed = torch.cat(channels + [clock_channel], dim=1)

        s = F.relu(self.bn1(self.conv1(s_transformed)))
        s = F.relu(self.bn2(self.conv2(s)))
        s = F.relu(self.bn3(self.conv3(s)))
        s = F.relu(self.bn4(self.conv4(s)))

        s = s.view(-1, self.args.num_channels * 12 * 12)

        s = F.dropout(F.relu(self.fc_bn1(self.fc1(s))), p=self.args.dropout, training=self.training)
        s = F.dropout(F.relu(self.fc_bn2(self.fc2(s))), p=self.args.dropout, training=self.training)

        pi = self.fc3(s)
        v = self.fc4(s)

        return F.log_softmax(pi, dim=1), torch.tanh(v)