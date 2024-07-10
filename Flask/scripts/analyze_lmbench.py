#!/usr/bin/python3 -u
import os
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

current_file_path = os.path.realpath(__file__)
current_directory = os.path.dirname(current_file_path)
test_cases = 4
bench_path = "/../host_lmbench/benchmarks_03_16_monitor_one_2/"

def find_table(data: str, name: str, cases: int) -> np.ndarray:
    start_index = data.find(name)
    data_starting = data[start_index:]
    lines = [line.strip() for line in data_starting.split("\n") if line.strip()]

    data_lines = lines[5 : 5 + cases]

    table = []

    for line in data_lines:
        values = line.split()
        table.append(values[3:])
    table = np.char.replace(np.array(table).T, "K", "")
    return table.astype(float)


def get_bcmk(file: str, cases: int) -> dict[str, np.ndarray]:

    with open(file, "r") as f:
        data = f.read()

    results = {}

    table = find_table(
        data,
        "*Local* Communication latencies in microseconds - smaller is better",
        cases,
    )
    data_pipe = table[0]
    data_tcp = table[2]

    table = find_table(
        data, "Processor, Processes - times in microseconds - smaller is better", cases
    )
    data_proc = table[9]
    data_stat = table[3]
    data_siginst = table[6]
    data_sighndl = table[7]

    table = find_table(
        data, "*Local* Communication bandwidths in MB/s - bigger is better", cases
    )
    data_read = table[6]
    data_write = table[7]

    table = find_table(
        data, "File & VM system latencies in microseconds - smaller is better", cases
    )
    data_mmap = table[4]
    data_fstat = table[7]

    results["pipe"] = data_pipe
    results["tcp"] = data_tcp
    results["proc"] = data_proc
    results["read"] = 1 / data_read
    results["write"] = 1 / data_write
    results["stat"] = data_stat
    results["fstat"] = data_fstat
    results["mmap"] = data_mmap
    results["siginst"] = data_siginst
    results["sighndl"] = data_sighndl

    return results


def fetch_data(file: str, cases: int):
    file = (
        current_directory + bench_path + file
    )
    raw = get_bcmk(file, cases)

    # IQR wash
    cleaned_data = {}

    for key, values in raw.items():
        Q1 = np.percentile(values, 25)
        Q3 = np.percentile(values, 75)
        IQR = Q3 - Q1

        lower_bound = Q1 - 1.5 * IQR
        upper_bound = Q3 + 1.5 * IQR

        non_outlier_values = values[(values >= lower_bound) & (values <= upper_bound)]

        # 计算非异常值的平均值
        mean_value = np.mean(non_outlier_values)

        # 用非异常值的平均值替换异常值
        replaced_values = np.where(
            (values < lower_bound) | (values > upper_bound), mean_value, values
        )
        cleaned_data[key] = replaced_values

    return pd.DataFrame(cleaned_data)


if __name__ == "__main__":
    baseline_avg = fetch_data("baseline0.txt", test_cases).mean()
    # flask0_avg = fetch_data("flask0.txt", test_cases).mean()
    flask10_avg = fetch_data("flask10.txt", test_cases).mean()
    flask20_avg = fetch_data("flask20.txt", test_cases).mean()
    flask30_avg = fetch_data("flask30.txt", test_cases).mean()
    flask40_avg = fetch_data("flask40.txt", test_cases).mean()

    # Normalize
    # baseline = baseline_avg / baseline_avg
    # flask0 = flask0_avg / baseline_avg
    flask10 = (flask10_avg / baseline_avg -1) * 100
    flask20 = (flask20_avg / baseline_avg -1) * 100
    flask30 = (flask30_avg / baseline_avg -1) * 100
    flask40 = (flask40_avg / baseline_avg -1) * 100

    df = pd.DataFrame(
        {
            # "baseline": baseline,
            # "flask0" : flask0,
            "flask10": flask10,
            "flask20": flask20,
            "flask30": flask30,
            "flask40": flask40,
        },
        index=[
            "pipe",
            "tcp",
            "proc",
            "read",
            "write",
            "stat",
            "fstat",
            "mmap",
            "siginst",
            "sighndl",
        ],
    )
    df.rename(index={"proc": "fork+exec"}, inplace=True)
    df.loc["avg"] = df.mean()
    print(df)

    # 定义每组数据的颜色
    patterns = ["////", "xxxx", "oo", "...."]
    # patterns = ["", "////", "xxxx", "oo", "....", "**"]

    # 创建图表
    plt.rcParams.update({"font.size": 14})

    fig, ax = plt.subplots(figsize=(8, 4))

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

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines['bottom'].set_visible(False)
    ax.spines['left'].set_visible(False)

    # 设置图例放置在图表上方，并使其水平排列
    ax.legend(
        loc="upper center",
        bbox_to_anchor=(0.5, 1.1),
        ncol=len(df.columns),
        frameon=False,
    )

    # 设置纵轴标题和范围
    ax.set_ylabel("Overhead(%)")
    ax.set_ylim(-1, 5)

    # 设置横轴标题和刻度标签
    ax.set_xticks(bar_positions + bar_width * (len(df.columns) - 1) / 2)
    ax.set_xticklabels(df.index)

    ax.tick_params(axis='x', length=0)
    ax.tick_params(axis='y', length=0)

    # 移除纵向网格线并设置水平网格线
    ax.yaxis.grid(True, linestyle="--", linewidth=0.5, alpha=0.7)
    # ax.xaxis.grid(False)

    plt.xticks(rotation=45)

    # 显示图表
    plt.tight_layout()
    plt.savefig("./benchmark.pdf", format="pdf")
    plt.show()
