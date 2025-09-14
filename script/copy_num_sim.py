# Re-importing necessary libraries after code execution reset
import matplotlib.pyplot as plt
import numpy as np

def compute_average_copies(read_write_ratio, max_requests=100):
    """
    计算在给定读写比下，GMem 数据块被失效前的平均副本数。
    
    :param read_write_ratio: 读写比 (例如：读:写 = 3:1, 则 read_write_ratio = 3)
    :param max_requests: 最大请求数
    :return: 各请求数下的平均副本数列表
    """
    # 计算读和写的概率
    read_prob = read_write_ratio / (read_write_ratio + 1)
    write_prob = 1 / (read_write_ratio + 1)

    average_copies_list = []

    for requests in range(1, max_requests + 1):
        average_copies = 0
        for copies in range(requests + 1):
            if copies == 0:
                prob_current = write_prob  # P(第一个请求是写请求)
            else:
                prob_current = (read_prob ** copies) * write_prob  # P(前 copies 次是读请求 & 第 (copies + 1) 次是写请求)
            average_copies += copies * prob_current
        average_copies_list.append(average_copies)

    return average_copies_list

# 绘制不同读写比的曲线
read_write_ratios = [1, 2, 3, 5, 10]
max_requests = 100

plt.figure(figsize=(10, 6))

for rwr in read_write_ratios:
    average_copies = compute_average_copies(rwr, max_requests)
    plt.plot(range(1, max_requests + 1), average_copies, label=f'Read:Write = {rwr}:1')

# 添加图例、标题和标签
plt.legend()
plt.title('Average Copies vs Number of Requests for Different Read-Write Ratios')
plt.xlabel('Number of Requests')
plt.ylabel('Average Copies')
plt.grid(True)

# 显示图表
plt.show()
