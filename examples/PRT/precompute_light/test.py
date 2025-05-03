import numpy as np

# 读取二进制文件
output_file = "E:\\aaaPROJECT\\GAMES202\\assets\\models\\sphere.sh9.bin"
array = np.fromfile(output_file, dtype=np.float32)  # 指定数据类型为 float32

# 重塑为原始形状 [3, 9]
array = array.reshape((3, 9))

print("读取的 NumPy 数组：")
print(array)