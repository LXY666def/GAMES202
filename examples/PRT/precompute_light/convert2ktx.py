import argparse
import os
import subprocess
import imageio as iio
from pathlib import Path

def convert_to_jpg(input_path, output_path):
    """
    使用 imageio 将图片转换为 JPG 格式。
    """
    try:
        # 读取输入图片
        image = iio.imread(input_path)
        # 保存为 JPG 格式
        iio.imwrite(output_path, image, format="jpg")
        print(f"转换成功：{input_path} -> {output_path}")
    except Exception as e:
        print(f"转换失败：{e}")
        exit(1)

def convert_to_ktx(input_path, output_path):
    """
    使用系统命令将图片转换为 KTX 格式。
    """
    try:
        subprocess.run(["toktx", output_path, input_path], check=True)
        print(f"转换成功：{input_path} -> {output_path}")
    except subprocess.CalledProcessError as e:
        print(f"转换失败：{e}")
        exit(1)

def main():
    # 使用 argparse 处理命令行参数
    parser = argparse.ArgumentParser(description="将图片转换为 JPG 或 KTX 格式")
    parser.add_argument("input_path", type=str, help="输入图片的路径")
    parser.add_argument("output_path", type=str, help="输出图片的路径")
    args = parser.parse_args()

    input_path = Path(args.input_path)
    output_path = Path(args.output_path)

    if not input_path.exists():
        print("错误：输入的文件不存在！")
        return

    # 检查文件扩展名是否为 HDR 类型
    hdr_extensions = {".hdr", ".exr"}  # HDR 类型的扩展名
    if input_path.suffix.lower() in hdr_extensions:
        # 如果是 HDR 类型，先转换为临时的 JPG 文件
        temp_jpg_path = input_path.with_suffix(".jpg")
        print(f"检测到 HDR 文件，先转换为临时 JPG 文件：{temp_jpg_path}")
        convert_to_jpg(input_path, temp_jpg_path)

        # 再将 JPG 文件转换为 KTX 文件
        convert_to_ktx(temp_jpg_path, output_path)

        # 删除临时 JPG 文件
        os.remove(temp_jpg_path)
        print(f"临时文件已删除：{temp_jpg_path}")
    else:
        # 如果不是 HDR 类型，直接转换为 JPG 文件
        output_jpg_path = output_path.with_suffix(".jpg")
        print(f"检测到非 HDR 文件，直接转换为 JPG 文件：{output_jpg_path}")
        convert_to_ktx(input_path, output_jpg_path)

    print(f"最终输出文件：{output_path}")

if __name__ == "__main__":
    main()