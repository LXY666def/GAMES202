import struct
from tqdm import tqdm

import matplotlib.pyplot as plt
import numpy as np
from precompute_light import SHBasis, uniformSampleOnSphere, sample_hdr_image, SHBasis_image

def read_matrices_from_file(filename):
    with open(filename, "rb") as file:
        num_matrices = struct.unpack("27f", file.read(4*27))
    return np.array(num_matrices, dtype=np.float32).reshape([3, 9]).T

def main():
    filename = "blue_studio.bin"
    SHparam = read_matrices_from_file(filename)
    width, height = 100, 50
    sh_value = SHBasis_image(width, height)
    color = (sh_value[..., None] * SHparam).sum(axis=2)

    plt.imshow(color)
    plt.show()

    print(f"read vertex num: {len(SHparam)}")


if __name__ == "__main__":
    main()