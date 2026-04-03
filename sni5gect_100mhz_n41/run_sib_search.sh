#!/bin/bash

# 检查是否提供文件夹参数
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <folder_path>"
    exit 1
fi

FOLDER="$1"

# 检查文件夹是否存在
if [ ! -d "$FOLDER" ]; then
    echo "Error: Directory '$FOLDER' does not exist."
    exit 1
fi

# 启用扩展 glob（可选，但更安全）
shopt -s nullglob

# 遍历所有匹配 sf_*_*_ch_*.fc32 的文件
for file in "$FOLDER"/sf_*_*_ch_*.fc32; do
    # 跳过无匹配的情况
    [ -e "$file" ] || continue

    filename=$(basename "$file")
    
    # 使用正则提取 task_idx, slot_idx, ch
    if [[ $filename =~ ^sf_([0-9]+)_([0-9]+)_ch_(-?[0-9]+)\.fc32$ ]]; then
        task_idx="${BASH_REMATCH[1]}"
        slot_idx="${BASH_REMATCH[2]}"
        ch="${BASH_REMATCH[3]}"

        echo "Processing: $file"
        echo "  task_idx=$task_idx, slot_idx=$slot_idx, ch=$ch"

        # 执行测试命令（-c 7 固定，-i 文件路径，-s slot_idx）
        ./build/shadower/test/sib_search_test -c 7 -i "$file" -s "$slot_idx"
        
        # 可选：检查上一条命令是否失败并退出
        # if [ $? -ne 0 ]; then
        #     echo "Command failed for $file, stopping."
        #     exit 1
        # fi
    else
        echo "Skipping (name mismatch): $filename"
    fi
done

echo "All done."