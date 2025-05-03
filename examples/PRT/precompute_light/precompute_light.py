import numpy as np
import imageio
import matplotlib.pyplot as plt

SH9 = np.array([
    0.28209479177387814,
    0.4886025119029199, 0.4886025119029199, 0.4886025119029199,
    1.0925484305920792, 1.0925484305920792, 0.31539156525252005, 1.0925484305920792, 0.5462742152960396
], dtype=np.float32)

def SHBasis(x, y, z):
    """
    compute degree 0-2 SH basis function value at given (x, y, z)
    :param x: [...] or [..., 1]
    :param y: [...] or [..., 1]
    :param z: [...] or [..., 1]
    :return: [..., 9]
    """
    if x.shape[-1] != 1:
        x = x[..., None]
    if y.shape[-1] != 1:
        y = y[..., None]
    if z.shape[-1] != 1:
        z = z[..., None]
    xx, yy, zz = x**2, y**2, z**2
    xy, yz, zx = x*y, y*z, z*x
    ones = np.ones_like(x)
    return np.concatenate([
        SH9[0] * ones,
        SH9[1] * y,
        SH9[2] * z,
        SH9[3] * x,
        SH9[4] * xy,
        SH9[5] * yz,
        SH9[6] * (3 * zz - 1),
        SH9[7] * zx,
        SH9[8] * (xx - yy)
    ], axis=-1)
def uniformSampleOnSphere(num_samples):
    """
    uniformly sample on the unit sphere
    :param num_samples: int
    :return: (x, y, z):3D coordinate, (u, v):texture coordinate
    """
    samples = np.random.rand(num_samples, 2).astype(np.float32)
    theta = np.arccos(2 * samples[:, 1:2] - 1)  # [0, pi]
    phi = 2 * np.pi * samples[:, 0:1]  # [0, 2*pi]
    u = samples[:, 0]
    v = np.arccos(2 * samples[:, 1] - 1) / np.pi
    x = np.cos(phi) * np.sin(theta)
    y = np.cos(theta)
    z = np.sin(phi) * np.sin(theta)

    return x, y, z, u, v
def sample_hdr_image(hdr_image, u, v):
    """
    bilinearly sample an image
    :param hdr_image: [H, W, channel]
    :param u: [N]
    :param v: [N]
    :return: [N, channel]
    """
    height, width, _ = hdr_image.shape

    x = u * (width - 1)
    y = v * (height - 1)

    x0, y0 = np.floor(x).astype(np.int32), np.floor(y).astype(np.int32)
    x1, y1 = np.clip(x0 + 1, 0, int(width) - 1), np.clip(y0 + 1 , 0, int(height) - 1)

    Ia = hdr_image[y0, x0]
    Ib = hdr_image[y1, x0]
    Ic = hdr_image[y0, x1]
    Id = hdr_image[y1, x1]

    wa = (x1 - x) * (y1 - y)
    wb = (x1 - x) * (y - y0)
    wc = (x - x0) * (y1 - y)
    wd = (x - x0) * (y - y0)

    return Ia * wa[..., None] + Ib * wb[..., None] + Ic * wc[..., None] + Id * wd[..., None]
def project2SHSpace(env_map, num_samples=10000):
    """
    compute sh params for a image
    :param env_map: [H, W, channel]
    :param num_samples: int
    :return: [9, channel]
    """
    height, width, channel = env_map.shape

    x, y, z, u, v = uniformSampleOnSphere(num_samples)

    SHValue = SHBasis(x, y, z)  # [N, 9]
    lightValue = sample_hdr_image(env_map, u, v)  # [N, channel]
    result = SHValue[:, :, None] * lightValue[:, None, :]  # [N, 9, channel]
    result = result.sum(axis=0) * 4 * np.pi / num_samples
    return result
def SHBasis_image(width, height):
    """
    compute degree 0-2 SH basis function value for an image
    :param width: int
    :param height: int
    :return: [H, W, 9]
    """
    u = np.linspace(0, 1, width)
    v = np.linspace(0, 1, height)
    u, v = np.meshgrid(u, v)
    theta = v * np.pi
    phi = u * 2 * np.pi
    x = np.cos(phi) * np.sin(theta)
    y = np.cos(theta)
    z = np.sin(phi) * np.sin(theta)
    sh_value = SHBasis(x, y, z)
    return sh_value
def aces_tone_mapping(image):
    """
    :param image: [H, W, 3]
    :return: [H, W, 3]
    """
    def adapted_lum(image):
        """
        calculate mean luminance
        :param image: [H, W, 3]
        :return: float
        """
        luminance = 0.299 * image[..., 0] + 0.587 * image[..., 1] + 0.114 * image[..., 2]
        adapted_lum = np.mean(luminance)
        return adapted_lum
    adapted_image = image * adapted_lum(image)
    return (adapted_image * (2.51 * adapted_image + 0.03)) / (adapted_image * (2.43 * adapted_image + 0.59) + 0.14)
if __name__ == '__main__':
    apply_log = False
    apply_tone_mapping = True
    env_map = np.array(imageio.imread('moon_noon.hdr'), dtype=np.float32)
    env_map_log = np.log(env_map + 1)
    env_map_tone = aces_tone_mapping(env_map)
    sh_coordinates = project2SHSpace(
        env_map_log if apply_log else env_map_tone if apply_tone_mapping else env_map, 1000000)

    height, width, channel = env_map.shape
    sh_value = SHBasis_image(width, height)
    color_log = (sh_value[..., None] * sh_coordinates).sum(axis=2)
    color = (np.exp(color_log) - 1) if apply_log else color_log

    print("SH params: ", sh_coordinates)

    #out2file = sh_coordinates.T.astype(np.float32).tofile("red_blue2.bin")

    fig, axs = plt.subplots(2, 1, figsize=(8, 8))
    axs[0].imshow(env_map)
    axs[0].set_title('HDR Image 1')
    axs[0].axis('off')
    axs[1].imshow(color)
    axs[1].set_title('HDR Image 2')
    axs[1].axis('off')
    plt.tight_layout()
    plt.show()

