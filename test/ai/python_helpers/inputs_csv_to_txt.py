import numpy as np


dataset = "cifar"

if dataset == "mnist":
    # Input CSV file (each row is an MNIST image)
    input_csv = "test/ai/data/inputs/mnist_test.csv"
    output_txt = "test/ai/data/inputs/mnist_test.txt"

    # Load CSV
    data = np.loadtxt(input_csv, delimiter=",")

    # Scale pixels to 0–1
    data_scaled = data / 255.0
    data_scaled[:, 0] = data_scaled[:, 0] * 255.0

    # Save to text file with spaces instead of commas
    np.savetxt(output_txt, data_scaled, fmt="%.6f", delimiter=" ")
    
else:
    # Input CSV file (each row is an CIFAR image)
    is_conv = False
    input_csv = "test/ai/data/inputs/cifar10_test.csv"
    output_txt = "test/ai/data/inputs/cifar10_test" + (".txt" if is_conv else "_nonconv.txt")

    # Load CSV
    data = np.loadtxt(input_csv, delimiter=",")
    
    means = [0.4914, 0.4822, 0.4465]
    stds = [0.2023, 0.1994, 0.2010]
    
    data_scaled = []
    for d in data:
        image = d[1:]/255
        temp = np.zeros(3072)
        
        count = 0
        for i in range(1024):
            temp[count] = (image[count] - means[0])/stds[0]
            count = count + 1
            temp[count] = (image[count] - means[1])/stds[1]
            count = count + 1
            temp[count] = (image[count] - means[2])/stds[2]
            count = count + 1
            
        if not is_conv:
            count = 0
            temp_nonconv = np.zeros(3072)
            for i in range(1024):
                temp_nonconv[i] = temp[count]       # R
                count += 1
                
                temp_nonconv[i + 1024] = temp[count] # G
                count += 1
                
                temp_nonconv[i + 2048] = temp[count] # B
                count += 1
                
            image_scaled = [int(d[0])] + temp_nonconv.tolist()
            
            data_scaled.append(image_scaled)
        else:
            image_scaled = [int(d[0])] + temp.tolist()
            data_scaled.append(image_scaled)
    

    # Save to text file with spaces instead of commas
    np.savetxt(output_txt, data_scaled, fmt="%.6f", delimiter=" ")
    
