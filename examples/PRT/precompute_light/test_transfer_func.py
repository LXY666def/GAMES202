import struct
from tqdm import tqdm

import matplotlib.pyplot as plt
import numpy as np
from precompute_light import SHBasis, uniformSampleOnSphere, sample_hdr_image, SHBasis_image

def read_matrices_from_file(filename):
    with open(filename, "rb") as file:
        num_matrices = struct.unpack("Q", file.read(8))[0]

        sh9s = []
        for _ in range(num_matrices):
            data = file.read(36)
            sh9 = struct.unpack("9f", data)
            # 转换为 3x3 矩阵
            sh9 = np.array(sh9, dtype=np.float32)
            sh9s.append(sh9)
    return sh9s

def main():
    filename = "E:\\aaaPROJECT\\GAMES202\\assets\\models\\sphere.sh9.bin"
    SHparam = read_matrices_from_file(filename)
    width, height = 100, 50
    shValue = SHBasis_image(width, height)
    plt.ion()
    for coord in tqdm(SHparam):
        projectImage = (shValue * coord).sum(axis=-1, keepdims=True)

        plt.cla()
        plt.imshow(projectImage, cmap='gray')
        plt.pause(0.1)

    print(f"read vertex num: {len(SHparam)}")


if __name__ == "__main__":
    main()