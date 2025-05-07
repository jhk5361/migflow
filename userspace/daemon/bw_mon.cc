#include <iostream>
#include <pqos.h>
#include <unistd.h>
#include <vector>

// 모니터링할 PID 리스트 설정
std::vector<pid_t> pids = {821382};  // 모니터링할 프로세스 ID (예: 12345)

int main() {
    // pqos 초기화 설정
    struct pqos_config cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.fd_log = STDOUT_FILENO;
    cfg.verbose = 0;

    // pqos 초기화
    int ret = pqos_init(&cfg);
    if (ret != PQOS_RETVAL_OK) {
        std::cerr << "Error initializing PQoS library!\n";
        return 1;
    }

    // PID 모니터링 그룹 생성
    struct pqos_mon_data *mon_data = nullptr;
    ret = pqos_mon_start_pids(pids.size(), pids.data(), PQOS_MON_EVENT_LMEM_BW, NULL, &mon_data);
    if (ret != PQOS_RETVAL_OK) {
        std::cerr << "Error starting PID monitoring!\n";
        pqos_fini();
        return 1;
    }

    // 실시간 메모리 대역폭 모니터링
    for (int i = 0; i < 1000; ++i) {
        sleep(1);  // 1초마다 업데이트
        ret = pqos_mon_poll(&mon_data, 1);
        if (ret != PQOS_RETVAL_OK) {
            std::cerr << "Error polling PID monitoring data!\n";
            break;
        }

        // 메모리 대역폭 결과 출력
        std::cout << "Memory Bandwidth (bytes): "
                  << mon_data->values.llc_misses << std::endl;
    }

    // 모니터링 종료
    ret = pqos_mon_stop(mon_data);
    if (ret != PQOS_RETVAL_OK) {
        std::cerr << "Error stopping PID monitoring!\n";
    }

    // pqos 종료
    pqos_fini();
    return 0;
}

