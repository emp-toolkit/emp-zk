params_file = "test/ai/data/parameters/cifar_relu_4_100.txt"
num_neurons_in_first_layer = 100

new_lines = []
with open(params_file, "r") as file:
    lines = file.readlines()
    for i, line in enumerate(lines[:num_neurons_in_first_layer]):
        sline = line.split(' ')
        rline = []
        for j in range(1024):
            rline.append(sline[j])
            rline.append(sline[j+1024])
            rline.append(sline[j+2048])
            
        new_lines.append(" ".join(rline))
        new_lines.append("\n")
    
    print(len(new_lines))
    new_lines += lines[num_neurons_in_first_layer:]
    
print(len(new_lines))

new_params_file = "test/ai/data/parameters/cifar_relu_4_100_1.txt"
with open(new_params_file, "w") as file:
    file.writelines(new_lines)