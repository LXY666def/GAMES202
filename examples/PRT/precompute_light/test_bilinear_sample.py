import numpy as np
import matplotlib.pyplot as plt
import imageio
from precompute_light import SHBasis, uniformSampleOnSphere, sample_hdr_image


width, height = 512, 256
image = np.zeros((height, width, 3), dtype=np.float32)

x = np.linspace(0, 1, width)
y = np.linspace(0, 1, height)
u, v = np.meshgrid(x, y)

env_map = imageio.imread('envmap.hdr')
sampled_image = sample_hdr_image(env_map, u, v)

theta = v * np.pi
phi = u * 2 * np.pi

image[:, :, 0] = np.cos(phi) * np.sin(theta)
image[:, :, 1] = np.sin(phi) * np.sin(theta)
image[:, :, 2] = np.cos(theta)

fig, axs = plt.subplots(2, 1, figsize=(8, 8))
axs[0].imshow(env_map)
axs[0].set_title('HDR Image origin')
axs[0].axis('off')
axs[1].imshow(sampled_image)
axs[1].set_title('HDR Image sampled')
axs[1].axis('off')
plt.tight_layout()
plt.show()