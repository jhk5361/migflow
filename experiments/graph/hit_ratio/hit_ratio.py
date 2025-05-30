import os
import re
import pandas as pd

from collections import defaultdict


# 벤치마크 이름 매핑
benchmark_name_map = {
    "dbtest": "Silo-TPCC",
    "redis-server": "Redis-YCSB",
    "omp-csr": "Graph500",
    "bench_btree": "Btree",
    "XSBench": "XSBench"
}

# 사용할 필드
field_colors = {
    "acc_nr_dram": "tab:blue",
    "acc_nr_rdram": "tab:orange",
    "acc_nr_nvm": "tab:green",
    "acc_nr_rnvm": "tab:red"
}
field_labels = {
    "acc_nr_dram": "Tier-0",
    "acc_nr_rdram": "Tier-1",
    "acc_nr_nvm": "Tier-2",
    "acc_nr_rnvm": "Tier-3"
}

# 결과 저장: {technique: {benchmark: {field: value}}}
trace_results = defaultdict(lambda: defaultdict(lambda: defaultdict(int)))

# 기준 디렉토리
base_dir = '../data'


target_fields = ["acc_nr_dram", "acc_nr_rdram", "acc_nr_nvm", "acc_nr_rnvm"]
for technique in os.listdir(base_dir):
    technique_path = os.path.join(base_dir, technique)
    if not os.path.isdir(technique_path):
        continue

    for filename in os.listdir(technique_path):
        if not filename.endswith('.dmesg'):
            continue

        # 벤치마크 이름 추출
        for bench_key, display_name in benchmark_name_map.items():
            if filename.startswith(bench_key):
                filepath = os.path.join(technique_path, filename)

                with open(filepath, 'r') as f:
                    lines = f.readlines()
                    if not lines:
                        continue

                    # 뒤에서부터 탐색
                    field_values = {}
                    for line in reversed(lines):
                        for field in target_fields:
                            if field not in field_values:
                                match = re.search(rf"{field}:\s+(\d+)", line)
                                if match:
                                    field_values[field] = int(match.group(1))
                        if len(field_values) == len(target_fields):
                            break  # 모든 필드 다 찾았으면 중단

                    if field_values:
                        for field in target_fields:
                            trace_results[technique][display_name][field] += field_values.get(field, 0)
                break

# 1. 각 구성 비율 (%) 출력
for technique in trace_results:
    for benchmark in trace_results[technique]:
        field_data = trace_results[technique][benchmark]
        total = sum(field_data[field] for field in target_fields)
        print(f"\n{technique} - {benchmark} (total: {total})")
        for field in target_fields:
            percent = field_data[field] / total * 100 if total > 0 else 0
            print(f"  {field}: {field_data[field]} ({percent:.2f}%)")


technique_map = {
    "default": "Default",
    "autonuma": "Balanced-AutoNUMA",
    "memtiering-mglru": "Tiered-AutoNUMA",
    "tpp": "TPP",
    "autotiering": "AutoTiering",
    "mtm": "MTM",
    "ours-td-qd-cb": "MigFlow"
}

benchmarks = ["Redis-YCSB", "Btree", "Silo-TPCC", "Graph500", "XSBench"]
tiers = ["Tier-0", "Tier-1", "Tier-2", "Tier-3"]
techniques = list(technique_map.keys())

# Build table per benchmark
table_results = {}
for benchmark in benchmarks:
    df = pd.DataFrame(index=tiers, columns=[technique_map[t] for t in techniques])
    for tech in techniques:
        display_tech = technique_map[tech]
        if tech in trace_results and benchmark in trace_results[tech]:
            field_data = trace_results[tech][benchmark]
            total = sum(field_data.get(f, 0) for f in field_labels)
            for field, label in field_labels.items():
                val = field_data.get(field, 0)
                percent = (val / total * 100) if total > 0 else 0
                #df.at[label, display_tech] = round(percent, 2)
                df.at[label, display_tech] = val
    table_results[benchmark] = df

for bench, df in table_results.items():
    print(f"\n=== {bench} ===")
    print(df.to_csv(index=True))
