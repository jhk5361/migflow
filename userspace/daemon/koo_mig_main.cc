#include <cstdio>
#include <unistd.h>
#include <string.h>
#include <wait.h>
#include <signal.h>
#include <iostream>
#include <argp.h>
#include "koo_mig_inf.h"
#include "koo_mig.h"

using namespace std;

struct opts my_opts;

static struct argp_option koomig_options[] = {
    {.name = "enable-quick-demotion",
     .key = 'q',
     .arg = "enable/disable",
     .doc = "Enable the quick demotion"},
    {.name = "enable-demotion-for-alloc",
     .key = 'd',
     .arg = "enable/disable",
     .doc = "Enable the demotion for alloc"},
    {.name = "enable-mig",
     .key = 'm',
     .arg = "enable/disable",
     .doc = "Enable the migration"},
    {.name = "enable-cb-promo",
     .key = 'c',
     .arg = "enable/disable",
     .doc = "Enable the cost/benefit promotion"},
    {.name = "user-pebs",
     .key = 'p',
     .arg = "true/false",
	 .doc = "Eable the user PEBS"},
    {.name = "is-fork",
     .key = 'f',
     .arg = "true/false",
     .doc = "true if the process perform the fork() call"},
    {.name = "print-interval",
     .key = 'i',
     .arg = "seconds",
     .doc = "sec of the print interval"},
	{.name = "verbose-level",
	 .key = 'v',
	 .arg = "0,1,2",
	 .doc = "0 = no print, 1 = print key metric, 2 = debug",
	},
    {NULL},
};

static error_t parse_option(int key, char *arg, struct argp_state *state) {
    struct opts *opts = (struct opts *)state->input;

    switch (key) {
        // enable migration
    case 'm':
        opts->do_mig = atoi(arg);
        break;

        // enable quick demotion
    case 'q':
        opts->do_quick_demotion = atoi(arg);
        break;

        // enable cost/benefit promotion
	case 'c':
		opts->do_cb_promo = atoi(arg);
		break;


        // when the process performs fork()
        // it will be used to get the child pid
        // graph500
	case 'f':
		opts->is_fork = atoi(arg);
		break;

        // user PEBS
        // 0 = disable, 1 = enable
	case 'p':
		opts->do_pebs = atoi(arg);
		break;

        // enable demotion for alloc
        // 0 = disable, 1 = enable
	case 'd':
		opts->do_demotion_for_alloc = atoi(arg);
		break;

        // verbose level 
        // 0 = no print, 1 = error, 2 = key metric, 3 = debug, 4 = debug more
	case 'v':
		opts->verbose_level = atoi(arg);
		break;

        // print interval (in seconds)
        // -1 means no print
	case 'i':
		opts->print_itv = atoi(arg);
		break;

    case ARGP_KEY_ARG:
        if (state->arg_num)
            return ARGP_ERR_UNKNOWN;
        if (opts->exename == NULL) {
            /* remaining options will be processed in ARGP_KEY_ARGS */
            return ARGP_ERR_UNKNOWN;
        }
        break;

    case ARGP_KEY_ARGS:
        /* process remaining non-option arguments */
        opts->exename = state->argv[state->next];
        opts->idx = state->next;
        break;

    case ARGP_KEY_NO_ARGS:
    case ARGP_KEY_END:
        if (state->arg_num < 1)
            argp_usage(state);
        break;

    default:
        return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

int main (int argc, char *argv[]) {
	struct argp argp = {
        .options = koomig_options,
        .parser = parse_option,
        .args_doc = "[<program>]",
        .doc = "koo_mig -- Control heterogeneous multi-tiered memory migration policy",
    };

    /* default option values */
    my_opts.do_quick_demotion = 0;
    my_opts.do_demotion_for_alloc = 0;
    my_opts.do_mig = 0;
    my_opts.is_fork = 0;
    my_opts.print_itv = -1;
    my_opts.verbose_level = PRINT_NONE;

    argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &my_opts);

	printf("%d %d\n", my_opts.do_quick_demotion, my_opts.do_mig);

    argc -= my_opts.idx;
    argv += my_opts.idx;

	int pid = fork();


	if (pid < 0) {
		cerr << "Fork failed\n"; 
		return -1;
	}

	if (pid == 0) {
		execv(my_opts.exename, argv);
		perror(my_opts.exename);
		return -1;
	} else {
		signal(SIGINT, SIG_IGN);
		koo_mig_inf_init(pid, &my_opts);
		printf("koo_mig pid: %d, child pid: %d\n", getpid(), pid);
		wait(NULL);
		printf("wait() finished\n");
		destroy_koo_mig_inf();
	}
}