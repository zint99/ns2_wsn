import pandas as pd
import matplotlib.pyplot as plt

# 读取 delay_aodv.csv 文件
data_aodv = pd.read_csv('delay_aodv.csv', header=None)

# 读取 delay_aomdv.csv 文件
data_aomdv = pd.read_csv('delay_aomdv.csv', header=None)

k = 10  # 设置要提取的列索引（从0开始计数）

y_data_aomdv = data_aomdv.iloc[:11, k]  # 提取前10行的第k列数据
y_data_aodv = data_aodv.iloc[:11, k]  # 提取前10行的第k列数据

x_data = range(3, 3 * len(y_data_aodv) + 1, 3)  # x轴数据为3到30，步长为3

# 创建图表和子图
fig, ax = plt.subplots(figsize=(8, 6))

# 绘制点线图
ax.plot(x_data, y_data_aomdv, marker='o',
        linestyle='-', color='r', label='AOMDV')
ax.plot(x_data, y_data_aodv, marker='*',
        linestyle='-', color='b', label='AODV')

# 设置标题和标签
ax.set_title('节点最大移动速度10m/s下发包速率对平均端到端延时的影响', fontsize=16)
ax.set_xlabel('发包速率 (Packet/S)', fontsize=12)
ax.set_ylabel('平均端到端延时', fontsize=12)

# 设置刻度标签的字体大小
ax.tick_params(axis='x', labelsize=10)
ax.tick_params(axis='y', labelsize=10)

# 设置网格线
ax.grid(True, linestyle='--')

# 添加图例
ax.legend(fontsize=10)

# 自动调整子图边界
fig.tight_layout()

# 保存图表为PNG格式
plt.savefig('delay_plot.png', dpi=300)

# 显示图表
plt.show()
