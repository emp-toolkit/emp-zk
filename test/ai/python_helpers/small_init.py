import torch
import torch.nn as nn
import torch.onnx as onnx

class MLP(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(5, 3)
        self.fc2 = nn.Linear(3, 3)
        self.relu = nn.ReLU()

        # initialize weights & biases in [-1, 1]
        for m in self.modules():
            if isinstance(m, nn.Linear):
                nn.init.uniform_(m.weight, -1.0, 1.0)
                nn.init.uniform_(m.bias, -1.0, 1.0)

    def forward(self, x):
        x = self.relu(self.fc1(x))   # Affine → ReLU
        x = self.relu(self.fc2(x))   # Affine → ReLU
        return x

model = MLP()
model.eval()

dummy_input = torch.randn(1, 5)

onnx.export(
    model,
    dummy_input,
    "model.onnx",
    input_names=["input"],
    output_names=["output"],
    opset_version=17
)