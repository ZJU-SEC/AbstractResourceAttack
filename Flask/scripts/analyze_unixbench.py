#!/usr/bin/python3 -u

import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

current_file_path = os.path.realpath(__file__)
current_directory = os.path.dirname(current_file_path)

def fetch_data(file: str):
    file = current_directory + "/../host_unixbench/benchmarks_03_15_monitor_one/" + file
    return pd.read_csv(file).iloc[:, 1:].to_numpy().flatten()

if __name__ == "__main__":
    baseline_raw = fetch_data("baseline.csv")
    flask10_raw = fetch_data("flask10.csv")
    flask20_raw = fetch_data("flask20.csv")
    flask30_raw = fetch_data("flask30.csv")
    flask40_raw = fetch_data("flask40.csv")

    baseline = baseline_raw / baseline_raw
    flask10 =  baseline_raw / flask10_raw
    flask20 =  baseline_raw / flask20_raw
    flask30 = baseline_raw / flask30_raw
    flask40 = baseline_raw / flask40_raw

    df = pd.DataFrame(
        {
            "baseline": baseline,
            "flask10": flask10,
            "flask20": flask20,
            "flask30": flask30,
            "flask40": flask40,
        },
        index=[
            "string",
            "float",
            "execl",
            "read",
            "write",
            "pipe",
            "ctxsw",
            "proc",
            "shell",
            "syscall",
        ],
    )
    df.rename(index={"proc": "fork+exec"}, inplace=True)
    df.loc["avg"] = df.mean()
    print(df)

    # 定义每组数据的颜色
    patterns = ["", "////", "xxxx", "oo", "...."]
    # patterns = ["", "////", "xxxx", "oo", "....", "**"]

    # 创建图表
    plt.rcParams.update({"font.size": 14})

    fig, ax = plt.subplots(figsize=(10, 6))

    # 绘制条形图，每组数据使用不同的颜色
    bar_width = 0.15  # 条形宽度
    bar_positions = np.arange(len(df))  # 条形位置

    # 绘制条形图，每组数据使用不同的颜色
    for i, (column, pattern) in enumerate(zip(df.columns, patterns)):
        ax.bar(
            bar_positions + i * bar_width,
            df[column],
            width=bar_width,
            label=column,
            hatch=pattern,
            edgecolor="black",
            facecolor="none",
            alpha=1,
        )

    # 移除最上面的y坐标线
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    # 设置图例放置在图表上方，并使其水平排列
    ax.legend(
        loc="upper center",
        bbox_to_anchor=(0.5, 1.1),
        ncol=len(df.columns),
        frameon=False,
    )

    # 设置纵轴标题和范围
    ax.set_ylabel("Overhead")
    ax.set_ylim(0, 1.2)

    # 设置横轴标题和刻度标签
    ax.set_xticks(bar_positions + bar_width * (len(df.columns) - 1) / 2)
    ax.set_xticklabels(df.index)

    # 移除纵向网格线并设置水平网格线
    ax.yaxis.grid(True, linestyle="--", linewidth=0.5, alpha=0.7)
    ax.xaxis.grid(False)

    plt.xticks(rotation=45)

    # 显示图表
    plt.tight_layout()
    plt.savefig("./benchmark.pdf", format="pdf")
    plt.show()
