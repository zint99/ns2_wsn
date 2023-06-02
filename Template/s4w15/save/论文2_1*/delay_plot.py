import pandas as pd
import matplotlib.pyplot as plt

# 读取 delay_aodv.csv 文件
data_aodv = pd.read_csv('delay_aodv.csv')

# 读取 delay_aomdv.csv 文件
data_aomdv = pd.read_csv('delay_aomdv.csv')

# 提取第7列和第8列的数据
column_7_aomdv = data_aomdv.iloc[:, 6]
column_8_aodv = data_aodv.iloc[:, 7]

# 计算横坐标范围
x_range = range(1, len(column_7_aomdv) + 1)  # 假设数据从 x = 1.0 开始，以列数据长度为范围

# 创建图表和子图
fig, ax = plt.subplots(figsize=(8, 6))

# 绘制点线图
ax.plot(x_range, column_7_aomdv, marker='o',
        linestyle='-', color='r', label='AOMDV')
ax.plot(x_range, column_8_aodv, marker='*',
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
