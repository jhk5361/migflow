#include <numa.h>
#include <sched.h>
#include <iostream>
#include <stdint.h>
#include <x86intrin.h>
#include <cstring>  // memcpy
#include <chrono>
#include <numaif.h>
#include <unordered_map>
#include <unistd.h>
#include <climits>
using namespace std;
using namespace std::chrono;

#define PAGE_SIZE (4 * 1024)  // 4KB
#define ARRAY_SIZE (10ULL * 1024 * 1024 * 1024)  // 10GB
#define CPU_FREQUENCY 4.0  // CPU 주파수: 4.0 GHz
#define ITER 10000

void bind_to_cpu(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_id, &cpuset);
    sched_setaffinity(0, sizeof(cpuset), &cpuset);
}

void flush_cache(void* ptr) {
    for (size_t i = 0; i < PAGE_SIZE; i += 64) {  // 캐시 라인 크기(64바이트)
        _mm_clflush((char*)ptr + i);
    }
}

// lfence를 이용하여 시리얼라이즈
static inline void lfence(void) {
    __asm__ __volatile__(
        "lfence" : : : "memory"
    );
}

// rdtsc를 이용한 현재 타임스탬프 측정 함수
static inline uint64_t rdtsc(void) {
    uint32_t low, high;
    __asm__ __volatile__(
        "rdtsc" : "=a" (low), "=d" (high)
    );
    return ((uint64_t)high << 32) | low;
}

// rdtscp를 이용한 현재 타임스탬프 측정 함수
static inline uint64_t rdtscp(void) {
    uint32_t low, high;
    uint32_t aux;
    __asm__ __volatile__(
        "rdtscp" : "=a" (low), "=d" (high), "=c" (aux)
    );
    return ((uint64_t)high << 32) | low;
}

#define NR_MOVE_PAGES 512
long __move_pages(int pid, int count, void **target_pages, int *nodes, int *status) {
	int moved_pages = 0;

	if (pid == -1)
		return -1;

	static unordered_map<int,unsigned long> err_map;


    auto start = high_resolution_clock::now();

	while (moved_pages < count) {
		int nr_move_pages = min(count - moved_pages, NR_MOVE_PAGES);

		if (move_pages(pid, nr_move_pages, target_pages + moved_pages, nodes == NULL ? NULL : nodes + moved_pages, status + moved_pages, MPOL_MF_MOVE) < 0) {
			if (err_map.count(errno) == 0)
				err_map.insert({errno,1});
			else
				err_map[errno]++;

			break;
		}

		moved_pages += nr_move_pages;
	}

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);

	//printf("[__move_pages: %ldms] moved_pages: %d\n", duration.count(), moved_pages);
	for (auto err : err_map) {
		printf("errno: %d, cnt: %lu\n", err.first, err.second);
	}

	//return moved_pages;
	return duration.count();
}

int main() {
    // 4개의 노드에서 페이지 메모리 할당
    void* mem_src[4];
    void* mem_dst[4];
	void *mem[4];
    for (int i = 0; i < 4; ++i) {
        mem[i] = numa_alloc_onnode(ARRAY_SIZE, i);
        mem_src[i] = numa_alloc_onnode(ARRAY_SIZE, i);
        mem_dst[i] = numa_alloc_onnode(ARRAY_SIZE, i);
        std::memset(mem[i], 0, ARRAY_SIZE);  // 메모리 초기화
        std::memset(mem_src[i], 0, ARRAY_SIZE);  // 메모리 초기화
        std::memset(mem_dst[i], 0, ARRAY_SIZE);  // 메모리 초기화
    }

    uint64_t start, end;
    double latency_ns;
	char *off_mem;
	char *off_src;
	char *off_dst;

	double tot_latency_ns_mp[4][4] = {0.0,};
	double tot_latency_ns_mc[4][4] = {0.0,};

	void *target_pages[NR_MOVE_PAGES];
	int nodes[NR_MOVE_PAGES];
	int status[NR_MOVE_PAGES];

	int pid = getpid();

    // CPU를 소스 노드에 바인딩
    bind_to_cpu(0);

	unsigned long succ = 0, fail = 0;

    // 모든 NUMA 노드 간의 조합을 테스트 (총 16가지)
    for (int src_node = 0; src_node < 4; ++src_node) {
        for (int dst_node = 0; dst_node < 4; ++dst_node) {

			for (int i = 0; i < ITER; i++) {
				for (int j = 0; j < NR_MOVE_PAGES; j++) {
					off_mem = (char *)mem[src_node] + (int)(rand() % ARRAY_SIZE / 4096 * 4096);
					target_pages[j] = (void *)off_mem;
					nodes[j] = dst_node;
					status[j] = -1;
				}
				tot_latency_ns_mp[src_node][dst_node] += __move_pages(0, NR_MOVE_PAGES, target_pages, nodes, status) / (double)NR_MOVE_PAGES;

				for (int j = 0; j < NR_MOVE_PAGES; j++) {
					if (status[j] != dst_node) {
						fail++;
					} else 
						succ++;
					nodes[j] = src_node;
				}
				__move_pages(pid, NR_MOVE_PAGES, target_pages, nodes, status);

				/*
				off_src = (char *)mem[src_node] + (int)(rand() % ARRAY_SIZE / 4096 * 4096);
				off_dst = (char *)mem_dst[dst_node] + (int)(rand() % ARRAY_SIZE / 4096 * 4096);
				*/

	            //start = rdtsc();
				//lfence();
		        //memcpy(mem_dst[dst_node], mem_src[src_node], PAGE_SIZE);  // 한 페이지 복사
		        //memcpy(off_dst, off_src, PAGE_SIZE);  // 한 페이지 복사
				//lfence();
		        //end = rdtscp();

				//latency_ns = (end - start) / (CPU_FREQUENCY * 1e9) * 1e9;

				//tot_latency_ns[src_node][dst_node] += latency_ns;
			}
			/*
            // 캐시를 비우기 위해 소스 및 목적지 메모리 캐시 플러시
            flush_cache(mem_src[src_node]);
            flush_cache(mem_dst[dst_node]);

            // RDTSC로 측정 시작
            start = rdtsc();
			lfence();
            memcpy(mem_dst[dst_node], mem_src[src_node], PAGE_SIZE);  // 한 페이지 복사
			lfence();
            end = rdtscp();
            
            // 사이클 수 -> 나노초 변환
            latency_ns = (end - start) / (CPU_FREQUENCY * 1e9) * 1e9;
            
            std::cout << "Latency from NUMA node " << src_node << " to NUMA node " 
                      << dst_node << ": " << latency_ns << " ns" << std::endl;
			*/
        }
    }

    for (int src_node = 0; src_node < 4; ++src_node) {
        for (int dst_node = 0; dst_node < 4; ++dst_node) {

			for (int i = 0; i < ITER; i++) {
				off_src = (char *)mem_src[src_node] + (int)(rand() % ARRAY_SIZE / 4096 * 4096);
				off_dst = (char *)mem_dst[dst_node] + (int)(rand() % ARRAY_SIZE / 4096 * 4096);

	            start = rdtsc();
				lfence();
		        memcpy(off_dst, off_src, PAGE_SIZE);  // 한 페이지 복사
				lfence();
		        end = rdtscp();

				latency_ns = (end - start) / (CPU_FREQUENCY * 1e9) * 1e9;

				tot_latency_ns_mc[src_node][dst_node] += latency_ns;
			}
            
        }
    }


    // 메모리 해제
    for (int i = 0; i < 4; ++i) {
        numa_free(mem_src[i], ARRAY_SIZE);
        numa_free(mem_dst[i], ARRAY_SIZE);
        numa_free(mem[i], ARRAY_SIZE);
    }

	cout << succ << " " << fail << endl;
	cout << "move_pages (us)" << endl;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			latency_ns = tot_latency_ns_mp[i][j] / ITER;
			if (i == j)
				cout << UINT_MAX << " ";
			else
				cout << latency_ns * 1000 << " ";
		}
		std::cout << std::endl;
	}

	cout << "memcpy (us)" << endl;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			latency_ns = tot_latency_ns_mc[i][j] / ITER;
			if (i == j)
				cout << UINT_MAX << " ";
			else
				cout << latency_ns << " ";
		}
		std::cout << std::endl;
	}


    return 0;
}

