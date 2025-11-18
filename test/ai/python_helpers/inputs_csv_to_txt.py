import numpy as np

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
    input_csv = "test/ai/data/inputs/cifar_test.csv"
    output_txt = "test/ai/data/inputs/cifar_test.txt"

    # Load CSV
    data = np.loadtxt(input_csv, delimiter=",")

    # Scale pixels to 0–1
    data_scaled = data / 255.0
    data_scaled[:, 0] = data_scaled[:, 0] * 255.0
    
    

    # Save to text file with spaces instead of commas
    np.savetxt(output_txt, data_scaled, fmt="%.6f", delimiter=" ")