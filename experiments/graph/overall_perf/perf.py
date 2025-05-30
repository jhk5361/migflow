import os
import re
import numpy as np
import matplotlib.pyplot as plt
from collections import defaultdict

# 설정
base_dir = '../data'

# 벤치마크 이름 매핑
benchmark_name_map = {
    "dbtest": "Silo-TPCC",
    "redis-server": "Redis-YCSB",
    "omp-csr": "Graph500",
    "bench_btree": "Btree",
    "XSBench": "XSBench"
}

# 1. 성능 데이터 수집
raw_results = defaultdict(lambda: defaultdict(list))

for technique in os.listdir(base_dir):
    technique_path = os.path.join(base_dir, technique)
    if not os.path.isdir(technique_path):
        continue

    for filename in os.listdir(technique_path):
        if not filename.endswith('.txt'):
            continue

        for bench_key, display_name in benchmark_name_map.items():
            if filename.startswith(bench_key):
                file_path = os.path.join(technique_path, filename)
                with open(file_path, 'r') as f:
                    for line in f:
                        if 'execution_time' in line:
                            match = re.search(r'execution_time\s+([0-9.]+)', line)
                            if match:
                                exec_time = float(match.group(1))
                                raw_results[technique][display_name].append(exec_time)
                            break

# 2. 정규화 성능 계산 (default 기준)
normalized_results = defaultdict(dict)

for bench in benchmark_name_map.values():
    default_times = raw_results['default'].get(bench, [])
    if not default_times:
        continue
    default_avg = np.mean(default_times)

    for technique in raw_results:
        tech_times = raw_results[technique].get(bench, [])
        if not tech_times:
            continue
        tech_avg = np.mean(tech_times)
        normalized_perf = default_avg / tech_avg
        normalized_results[bench][technique] = normalized_perf

# 결과 출력
for bench in sorted(normalized_results):
    for technique in normalized_results[bench]:
        perf = normalized_results[bench][technique]
        etime = raw_results[technique][bench][0]
        print(f"{bench} - {technique}: normalized performance = {perf:.4f}, execution_time (s) = {etime}")


