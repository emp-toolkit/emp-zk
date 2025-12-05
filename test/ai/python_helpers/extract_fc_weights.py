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


model_path = "test/eran_models/cifar_relu_4_100.onnx"
output_path = "test/ai/data/parameters/cifar_relu_4_100_1.txt"

model, is_conv = read_onnx_net(model_path)
params = get_onnx_parameters_as_arrays(model)

num_layers = len(params.items())//2
params_array = []
for i in range(num_layers):
    params_array.append((f"{2*(i+1)}.weight", params[f"{2*(i+1)}.weight"]))
    params_array.append((f"{2*(i+1)}.bias", params[f"{2*(i+1)}.bias"]))


with open(output_path, "w") as file:
    for name, array in params_array:
        print("Shape:", array.shape)
        
        arr_str = ""
        if "weight" in name:
            m, n = array.shape
            for i in range(m):
                for j in range(n):
                    arr_str = arr_str + str(array[i][j])
                    arr_str = arr_str + " " 
                arr_str = arr_str + "\n"    
            # print(arr_str)        
        elif "bias" in name:
            m = array.shape[0]
            for i in array:
                arr_str += str(i)
                arr_str += " "
            # print(arr_str)
            arr_str += "\n"
        
        file.writelines(arr_str)
        
        print("---------")