#!/bin/bash

# 执行 lab1/run_lab1.sh
echo "Running lab1..."
./lab1/run_lab1.sh

# 检查 lab1 脚本的退出状态
if [ $? -eq 0 ]; then
  echo "Lab1 executed successfully"
else
  echo "Lab1 failed"
  exit 1
fi

# 执行 lab2/run_lab2.sh
echo "Running lab2..."
./lab2/run_lab2.sh

# 检查 lab2 脚本的退出状态
if [ $? -eq 0 ]; then
  echo "Lab2 executed successfully"
else
  echo "Lab2 failed"
  exit 1
fi

echo "All scripts executed successfully"
