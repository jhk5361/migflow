#include "koo_mig.h"
#include "koo_mig_inf.h"

extern "C" {
	int koo_mig_inf_init(int pid, void *opts) {
		return koo_mig_init(pid, opts);
	}

	void destroy_koo_mig_inf() {
		destroy_koo_mig();
	}
}
