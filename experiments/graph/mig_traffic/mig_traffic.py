import os
import re
from collections import defaultdict

# 벤치마크 이름 매핑
benchmark_name_map = {
    "dbtest": "Silo-TPCC",
    "redis-server": "Redis-YCSB",
    "omp-csr": "Graph500",
    "bench_btree": "Btree",
    "XSBench": "XSBench"
}

# 결과 저장 구조
migration_results = defaultdict(lambda: defaultdict(lambda: defaultdict(int)))
promotion_results = defaultdict(lambda: defaultdict(int))
demotion_results = defaultdict(lambda: defaultdict(int))

base_dir = "../data"

for technique in os.listdir(base_dir):
    tech_path = os.path.join(base_dir, technique)
    if not os.path.isdir(tech_path):
        continue

    for filename in os.listdir(tech_path):
        if not filename.endswith(".vmstat"):
            continue

        for bench_key, bench_name in benchmark_name_map.items():
            if filename.startswith(bench_key):
                filepath = os.path.join(tech_path, filename)
                with open(filepath, "r") as f:
                    lines = f.readlines()

                # migrate_*_* 라인만 추출
                migrate_lines = [line.strip() for line in lines if line.startswith("migrate_")]
                if len(migrate_lines) < 32:
                    continue  # 스냅샷 2개가 부족하면 skip

                first_snapshot_lines = migrate_lines[:16]
                second_snapshot_lines = migrate_lines[-16:]

                def parse_snapshot(lines):
                    parsed = {}
                    for line in lines:
                        match = re.match(r"migrate_(\d+)_(\d+)\s+(\d+)", line)
                        if match:
                            src, dst, val = int(match.group(1)), int(match.group(2)), int(match.group(3))
                            parsed[(src, dst)] = val
                    return parsed

                first_snapshot = parse_snapshot(first_snapshot_lines)
                second_snapshot = parse_snapshot(second_snapshot_lines)

                # 실제 트래픽 계산
                for key in second_snapshot:
                    delta = second_snapshot[key] - first_snapshot.get(key, 0)
                    migration_results[technique][bench_name][key] += delta
                    src, dst = key
                    if src > dst:
                        promotion_results[technique][bench_name] += delta
                    elif src < dst:
                        demotion_results[technique][bench_name] += delta
                break  # 벤치마크 매칭 완료되면 다음 파일로

# 결과 출력
for technique in migration_results:
    for benchmark in migration_results[technique]:
        print(f"\n=== {technique} - {benchmark} ===")
        #for (src, dst), count in migration_results[technique][benchmark].items():
        #    print(f"migrate_{src}_{dst}: {count}")
        print(f"> Promotion traffic: {promotion_results[technique][benchmark]}")
        print(f"> Demotion traffic: {demotion_results[technique][benchmark]}")

