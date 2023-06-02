import pandas as pd
import matplotlib.pyplot as plt

data_aodv = pd.read_csv('pdr_aodv.csv')

data_aomdv = pd.read_csv('pdr_aomdv.csv')

col_data_aomdv = data_aomdv.iloc[:, 5]
col_data_aodv = data_aodv.iloc[:, 5]

# 计算横坐标范围
x_range = range(1, len(col_data_aodv) + 1)  # 假设数据从 x = 1.0 开始，以列数据长度为范围

# 创建图表和子图
fig, ax = plt.subplots(figsize=(8, 6))

# 绘制点线图
ax.plot(x_range, col_data_aomdv, marker='o',
        linestyle='-', color='r', label='AOMDV')
ax.plot(x_range, col_data_aodv, marker='*',
        linestyle='-', color='b', label='AODV')

# 设置标题和标签
ax.set_title('节点最大移动速度10m/s下发包速率对包投递率的影响', fontsize=16)
ax.set_xlabel('发包速率 (Packet/S)', fontsize=12)
ax.set_ylabel('包投递率', fontsize=12)

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
plt.savefig('pdr_plot.png', dpi=300)

# 显示图表
plt.show()
