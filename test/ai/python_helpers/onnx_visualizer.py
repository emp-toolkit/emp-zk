import onnx
import numpy as np
from onnx import numpy_helper

def read_onnx_net(net_file):
    onnx_model = onnx.load(net_file)
    onnx.checker.check_model(onnx_model)

    is_conv = any(node.op_type == "Conv" for node in onnx_model.graph.node)
    return onnx_model, is_conv


def get_onnx_parameters_as_arrays(onnx_model):
    params = {}

    for init in onnx_model.graph.initializer:
        arr = numpy_helper.to_array(init)
        params[init.name] = arr

    return params


# Example usage:
model, is_conv = read_onnx_net("test/eran_models/mnist_relu_3_100.onnx")
params = get_onnx_parameters_as_arrays(model)

for name, array in params.items():
    print("Parameter:", name)
    print("Shape:", array.shape)
    print(array)
    print("---------")