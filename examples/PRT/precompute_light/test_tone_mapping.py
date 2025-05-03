import numpy as np
import imageio
import matplotlib.pyplot as plt

SH9 = np.array([
    0.28209479177387814,
    0.4886025119029199, 0.4886025119029199, 0.4886025119029199,
    1.0925484305920792, 1.0925484305920792, 0.31539156525252005, 1.0925484305920792, 0.5462742152960396
], dtype=np.float32)

from precompute_light import SHBasis, uniformSampleOnSphere, sample_hdr_image, SHBasis_image, project2SHSpace

def aces_tone_mapping(image):
    def adapted_lum(image):
        luminance = 0.299 * image[..., 0] + 0.587 * image[..., 1] + 0.114 * image[..., 2]
        adapted_lum = np.mean(luminance)
        return adapted_lum
    adapted_image = image * adapted_lum(image)
    return (adapted_image * (2.51 * adapted_image + 0.03)) / (adapted_image * (2.43 * adapted_image + 0.59) + 0.14)

if __name__ == '__main__':
    env_map = np.array(imageio.imread('blue_studio.hdr'), dtype=np.float32)
    color = aces_tone_mapping(env_map)
    color_rgba = np.concatenate([color, np.ones((color.shape[0], color.shape[1], 1))], axis=-1)
    imageio.imwrite('blue_studio.png', color_rgba)

    fig, axs = plt.subplots(2, 1, figsize=(8, 8))
    axs[0].imshow(env_map)
    axs[0].set_title('HDR Image 1')
    axs[0].axis('off')
    axs[1].imshow(color)
    axs[1].set_title('HDR Image 2')
    axs[1].axis('off')
    plt.tight_layout()
    plt.show()

