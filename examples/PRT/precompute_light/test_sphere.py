import test_envmap_sh
import test_transfer_func

filename = "blue_studio.bin"
SHparam = test_envmap_sh.read_matrices_from_file(filename)

filename = "E:\\aaaPROJECT\\GAMES202\\assets\\models\\sphere.sh9.bin"
SHparam1 = test_transfer_func.read_matrices_from_file(filename)

for geo in SHparam1:
    color = (SHparam.T * geo).sum(axis=-1)
    print(color)