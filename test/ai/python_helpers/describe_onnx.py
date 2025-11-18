import onnx
from onnx import helper, numpy_helper

# Load ONNX model
model_path = "test/eran_models/model.onnx"
# model = onnx.load(model_path)

# # Check model is valid
# onnx.checker.check_model(model)

# print("Model Graph Name:", model.graph.name)
# print("Number of nodes:", len(model.graph.node))
# print("\nLayers / Nodes with attributes:")

# for i, node in enumerate(model.graph.node):
#     print(f"Layer {i}:")
#     print("  Name:", node.name if node.name else "N/A")
#     print("  OpType:", node.op_type)
#     print("  Inputs:", node.input)
#     print("  Outputs:", node.output)
    
#     # Print node attributes
#     if node.attribute:
#         print("  Attributes:")
#         for attr in node.attribute:
#             # Convert attribute to readable form
#             value = helper.get_attribute_value(attr)
#             print(f"    {attr.name}: {value}")
#     else:
#         print("  Attributes: None")
    
#     print("-" * 50)

# Optional: Print input/output shapes if available
def get_value_info_shapes(value_info_list):
    shapes = {}
    for vi in value_info_list:
        if vi.type.HasField("tensor_type"):
            dims = [d.dim_value if d.dim_value > 0 else '?' for d in vi.type.tensor_type.shape.dim]
            shapes[vi.name] = dims
    return shapes


import onnx
from onnx2pytorch import ConvertModel
import torch

# Load the ONNX model
onnx_model = onnx.load(model_path)

# Convert to PyTorch
pytorch_model = ConvertModel(onnx_model, experimental=True)

# Set to eval mode
pytorch_model.eval()

print(pytorch_model)

# Test with a dummy input
# Replace input size with your model's expected shape
dummy_input = torch.randn(1, 1, 1, 5)  # e.g., MNIST
output = pytorch_model(dummy_input)
print("Output shape:", output.shape)
