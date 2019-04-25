/*
 * This program verifies the three counterexamples to dual priority conjectures
 * given in the paper entitled "Dual Priority Scheduling is Not Optimal".
 *
 * As this program serves the purpose of a proof, much of it is written in a
 * naive style in order to be as easy as possible for a human reader to
 * verify correct, even when this breaks usual good programming practices.
 * For example, as all the counterexamples have four tasks, this program is
 * hardcoded for task sets with exactly four tasks. Combinations and
 * permutations are generated with deep nested loops instead of with more
 * elegant (but more complicated) methods. To avoid any extra complexity, this
 * program is also single-threaded, despite the inherent parallelism that is
 * possible when testing vast numbers of configurations.
 *
 * This program should compile on any standards-compliant C99 compiler. High
 * compiler optimization settings (e.g., -O3 for gcc, clang) are recommended.
 *
 * All counterexamples have been verified with the below compilers and CPUs.
 *
 * Tested compilers: GCC 8.2.1/5.5.0/4.9.2 and Clang/LLVM 7.0.1
 * Tested CPUs: AMD Ryzen 7 1700X, AMD Opteron 2220 SE, and Intel Xeon E5520.
 *
 * Written by Pontus Ekberg <pontus.ekberg@it.uu.se>.
 * Last updated on February 5, 2019.
 */

#include <stdio.h>  /* For printf() */
#include <stdlib.h> /* For exit() and atoi() */
#include <assert.h> /* For assert() */

#define VERBOSE   0 /* Set to 1 for lots of output (will run MUCH slower) */
#define NUM_TASKS 4 /* Warning: WILL break for other values than 4 */

struct task_t {
	/* Fixed task parameters */
	int wcet;
	int period;

	/* Dual-priority parameters */
	int phase_1_prio;       /* Priority before phase change */
	int phase_2_prio;       /* Priority after phase change */
	int phase_change_point; /* Phase change point, relative to release time */

	/* Simulation state */
	long last_release_time; /* Time point of the task's last job release */
	int remaining_wcet;     /* Remaining execution of the task's last job */
};

struct taskset_t {
	struct task_t tasks[NUM_TASKS];
	long hyper_period;
};

/*
 * ============================================================================
 * Convenience functions.
 * ============================================================================
 */

long gcd(long a, long b) {
	long temp;
	while (b != 0) {
		temp = a % b;
		a = b;
		b = temp;
	}
	return a;
}

long lcm(long a, long b) {
	return a / gcd(a, b) * b; 
}

long hyper_period(struct taskset_t *ts) {
	long hp = 1;
	for (int i = 0; i < NUM_TASKS; i++) {
		hp = lcm(hp, ts->tasks[i].period);
	}
	return hp;
}

/*
 * Print the task set. Also prints current priority settings and phase change
 * points if print_prios and print_phase_change_points are true.
 */
void print_taskset(struct taskset_t *ts, int print_prios,
	   	int print_phase_change_points) {
	for (int i = 0; i < NUM_TASKS; i++) {
		printf("T%d (%2d, %3d):", 
				i+1,
			   	ts->tasks[i].wcet,
			   	ts->tasks[i].period);
		if (print_prios) {
			printf(" phase 1 prio = %d, phase 2 prio = %d",
					ts->tasks[i].phase_1_prio,
					ts->tasks[i].phase_2_prio);
		}
		if (print_phase_change_points) {
			printf(", phase change point = %d",
					ts->tasks[i].phase_change_point);
		}
		printf("\n");
	}
}

/*
 * ============================================================================
 * Functions for simulating dual-priority scheduling of the SAS.
 * ============================================================================
 */

/*
 * Reset the simulation state variables of all tasks.
 */
void reset_simulation_state(struct taskset_t *ts) {
	for (int i = 0; i < NUM_TASKS; i++) {
		ts->tasks[i].last_release_time = -1; /* -1 means never released */
		ts->tasks[i].remaining_wcet = -1;
	}
}

/*
 * Check if the task has an active job.
 */
int is_active(struct task_t *task) {
	return task->remaining_wcet > 0;
}

/*
 * Check if the task can release a new job at time point t.
 */
int can_release(struct task_t *task, long t) {
	if (task->last_release_time < 0) { /* Task has not released any job */
		return 1;
	}
	return t - task->last_release_time >= task->period;
}

/*
 * Release a new job from the task at time point t.
 */
void release(struct task_t *task, long t) {
	task->last_release_time = t;
	task->remaining_wcet = task->wcet;
}

/*
 * Check if the task has missed its deadline at time point t.
 */
int has_missed_deadline(struct task_t *task, long t) {
	return is_active(task) && can_release(task, t);
}

/*
 * Get the priority of the task at time point t.
 * Lower number is higher priority.
 */
int get_priority(struct task_t *task, long t) {
	if (t - task->last_release_time < task->phase_change_point) {
		return task->phase_1_prio;
	}
	return task->phase_2_prio;
}

/*
 * Get a pointer to the highest-priority active task at time point t.
 * Returns NULL if there are no active tasks.
 */
struct task_t *get_highest_prio_active_task(struct taskset_t *ts, long t) {
	int highest_prio = -1;
	struct task_t *hp_task = NULL;
	int prio;
	for (int i = 0; i < NUM_TASKS; i++) {
		if (is_active(&ts->tasks[i])) {
			prio = get_priority(&ts->tasks[i], t);
			if (prio < highest_prio || highest_prio < 0) {
				highest_prio = prio;
				hp_task = &ts->tasks[i];
			}
		}
	}
	return hp_task;
}

/*
 * Simulate the SAS up to the hyper-period or the first deadline miss. 
 * Returns a pointer to the first task to miss a deadline, or NULL if all
 * deadlines are met.
 */
struct task_t *simulate_sas(struct taskset_t *ts) {
	reset_simulation_state(ts);
	long t = 0;
	struct task_t *hp_task;

	while (t <= ts->hyper_period) {

		/* Check for deadline misses. */
		for (int i = 0; i < NUM_TASKS; i++) {
			if (has_missed_deadline(&ts->tasks[i], t)) {
				return &ts->tasks[i]; /* Return on first deadline miss. */
			}
		}

		/* Release new jobs from all ready tasks. */
		for (int i = 0; i < NUM_TASKS; i++) {
			if (can_release(&ts->tasks[i], t)) {
				release(&ts->tasks[i], t);
			}
		}

		/* Execute the highest-priority task and progress time. */
		hp_task = get_highest_prio_active_task(ts, t);
		if (hp_task != NULL) {
			hp_task->remaining_wcet--;
		}
		t++;
	}

	return NULL; /* No deadline misses in the SAS. */
}

/*
 * ============================================================================
 * Functions for exhaustively testing dual-priority schedulability.
 *
 * The below functionality is written in a naive style and is hard coded for
 * NUM_TASKS = 4 (which holds for all the counterexamples). For example, many
 * simple loops are unrolled, and combinations and permutations are generated
 * using nested for-loops instead of using more sophisticated methods.
 *
 * The intent with this naive coding is that it should be as easy as possible
 * for a (human) reader to verify the correctness of the code.
 * ============================================================================
 */

/*
 * Test whether the task set is dual-priority schedulable with the current 
 * priorities and phase change points set according to the FDMS policy.
 *
 * Returns 1 if the task set is schedulable using this policy, 0 otherwise.
 *
 * Precondition: The task set has four tasks, with phase 1 and phase 2
 *               priorities already set.
 *
 * The FDMS policy works as follows.
 *
 * (1) All the phase change points are set equal to the corresponding periods.
 * (2) The SAS is simulated for one hyper-period, or until a deadline miss.
 * (3) If there are no deadline misses in (2), the task set is schedulable.
 *     Otherwise, the phase change point of the first task to miss its deadline
 *     is decreased by 1. If the phase change point of that task was already 0,
 *     then the FDMS policy fails. Otherwise, repeat from (2).
 */
int test_fdms_phase_change_points(struct taskset_t *ts) {
	ts->tasks[0].phase_change_point = ts->tasks[0].period;
	ts->tasks[1].phase_change_point = ts->tasks[1].period;
	ts->tasks[2].phase_change_point = ts->tasks[2].period;
	ts->tasks[3].phase_change_point = ts->tasks[3].period;

	struct task_t *miss_task;
	while (1) {
		miss_task = simulate_sas(ts);
		if (miss_task == NULL) { /* No deadline miss, return success */
			return 1;
		} else if (miss_task->phase_change_point > 0) { /* Decrease point */
			miss_task->phase_change_point--;
		} else { /* Phase change point already zero, return failure */
			return 0;
		}
	}
}

/*
 * Test whether the task set is dual-priority schedulable with any setting
 * of the phase change points with the current priority ordering. The phase
 * change point of each task will range between 0 and the task's period. Works
 * by simulating the SAS until the first deadline miss for all settings of the 
 * phase change points, or until a schedulable configuration is found. 
 *
 * Returns 1 if any schedulable configuration of the phase change points exists
 * with the current priorities, returns 0 otherwise.
 *
 * Precondition: The task set has four tasks, with phase 1 and phase 2
 *               priorities already set.
 */
int test_all_phase_change_points(struct taskset_t *ts) {
	const long total_combinations =	(ts->tasks[0].period + 1) * 
	                                (ts->tasks[1].period + 1) * 
	                                (ts->tasks[2].period + 1) * 
	                                (ts->tasks[3].period + 1);
	long generated_combinations = 0;

	printf("Testing all %ld possible combinations of phase change points...\n",
			total_combinations);

	/*
	 * Naively generate all combinations of phase change points.
	 * T1pcp becomes the phase change point of task T1 etc.
	 */
	for (int T1pcp = 0; T1pcp <= ts->tasks[0].period; T1pcp++) {
		ts->tasks[0].phase_change_point = T1pcp;

		for (int T2pcp = 0; T2pcp <= ts->tasks[1].period; T2pcp++) {
			ts->tasks[1].phase_change_point = T2pcp;

			for (int T3pcp = 0; T3pcp <= ts->tasks[2].period; T3pcp++) {
				ts->tasks[2].phase_change_point = T3pcp;

				for (int T4pcp = 0; T4pcp <= ts->tasks[3].period; T4pcp++) {
					ts->tasks[3].phase_change_point = T4pcp;

					generated_combinations++;
					
					if (VERBOSE) {
						printf("Testing phase change point combination "
								"%ld of %ld...\n",
								generated_combinations,
							   	total_combinations);
						print_taskset(ts, 1, 1);
					}
					if (simulate_sas(ts) == NULL) { /* SAS is schedulable */
						printf("Schedulable with this configuration:\n\n");
						print_taskset(ts, 1, 1);
						return 1; /* Return if a valid setting is found. */
					} else if (VERBOSE) {
						printf("Unschedulable with this configuration.\n\n");
					}
				}
			}
		}
	}
	assert(generated_combinations == total_combinations);
	return 0; /* Not schedulable with any promotion points. */
}

/*
 * Test dual priority schedulability exhaustively by simulating the SAS for
 * all possible configurations of priorities and phase change points.
 *
 * Returns 1 if there exists a schedulable configuration, otherwise returns 0.
 *
 * Precondition: The task set has four tasks.
 *
 * There are 8! = 40320 possible permutations of the 8 priority values
 * (each task has 2 priorities). These will all be generated.
 *
 * For each priority permutation, test_all_phase_change_point_combinatios()
 * is called to simulate thet SAS with all possible settings of the phase
 * change points.
 */
int test_all_configurations_exhaustively(struct taskset_t *ts) {
	const long total_permutations = 8*7*6*5*4*3*2*1; /* 8! = 40320 */
	long generated_permutations = 0;

	/*
	 * Naively generate all permutations of priorities.
	 * T1p2 becomes the phase 2 priority of task T1 etc.
	 */
	for (int T1p2 = 0; T1p2 < 8; T1p2++) {
		ts->tasks[0].phase_2_prio = T1p2;

		for (int T2p2 = 0; T2p2 < 8; T2p2++) {
			if (T2p2 == T1p2) continue;
			ts->tasks[1].phase_2_prio = T2p2;

			for (int T3p2 = 0; T3p2 < 8; T3p2++) {
				if (T3p2 == T1p2) continue;
				if (T3p2 == T2p2) continue;
				ts->tasks[2].phase_2_prio = T3p2;

				for (int T4p2 = 0; T4p2 < 8; T4p2++) {
					if (T4p2 == T1p2) continue;
					if (T4p2 == T2p2) continue;
					if (T4p2 == T3p2) continue;
					ts->tasks[3].phase_2_prio = T4p2;

					for (int T1p1 = 0; T1p1 < 8; T1p1++) {
						if (T1p1 == T1p2) continue;
						if (T1p1 == T2p2) continue;
						if (T1p1 == T3p2) continue;
						if (T1p1 == T4p2) continue;
						ts->tasks[0].phase_1_prio = T1p1;

						for (int T2p1 = 0; T2p1 < 8; T2p1++) {
							if (T2p1 == T1p2) continue;
							if (T2p1 == T2p2) continue;
							if (T2p1 == T3p2) continue;
							if (T2p1 == T4p2) continue;
							if (T2p1 == T1p1) continue;
							ts->tasks[1].phase_1_prio = T2p1;

							for (int T3p1 = 0; T3p1 < 8; T3p1++) {
								if (T3p1 == T1p2) continue;
								if (T3p1 == T2p2) continue;
								if (T3p1 == T3p2) continue;
								if (T3p1 == T4p2) continue;
								if (T3p1 == T1p1) continue;
								if (T3p1 == T2p1) continue;
								ts->tasks[2].phase_1_prio = T3p1;

								for (int T4p1 = 0; T4p1 < 8; T4p1++) {
									if (T4p1 == T1p2) continue;
									if (T4p1 == T2p2) continue;
									if (T4p1 == T3p2) continue;
									if (T4p1 == T4p2) continue;
									if (T4p1 == T1p1) continue;
									if (T4p1 == T2p1) continue;
									if (T4p1 == T3p1) continue;
									ts->tasks[3].phase_1_prio = T4p1;
									
									generated_permutations++;

									printf("Generated priority permutation "
										"%ld of %ld...\n",
										generated_permutations, 
										total_permutations);
									print_taskset(ts, 1, 0);

									/*
									 * Test all possible combinations of phase
									 * change points with these priorities by
									 * simulating the SAS.
									 */
									if (test_all_phase_change_points(ts)) {
										return 1; /* Return if schedulable */
									}
									printf("Unschedulable for all combinations "
										   "of phase change points.\n\n");
								}
							}
						}
					}
				}
			}
		}
	}
	/* Not schedulable with any priority permutation */
	assert(generated_permutations == total_permutations);
	printf("Task set is not dual-priority schedulable!\n");
	return 0; 
}

/*
 * Test dual priority schedulability exhaustively by simulating the SAS for
 * all possible configurations of priorities and phase change points, under the
 * restriction that the phase 1 priorities of the tasks are Rate Monotonic.
 *
 * Returns 1 if there is such a schedulable configuration, otherwise returns 0.
 *
 * Precondition: The task set has four tasks sorted in Rate Monotonic order.
 *
 * There are binomial(8, 4) * 4! = 8! / 4! = 1680 possible priority
 * permutations here. These will all be generated.
 *
 * For each priority permutation, test_all_phase_change_point_combinatios()
 * is called to simulate thet SAS with all possible settings of the phase
 * change points.
 */
int test_rm_configurations_exhaustively(struct taskset_t *ts) {
	const long total_permutations = 8*7*6*5; /* 8!/4! = 1680 */
	long generated_permutations = 0;

	/*
	 * Naively generate all permutations of priorities, where the phase 1
	 * priorities are Rate Monotonic.
	 * T1p2 becomes the phase 2 priority of task T1 etc.
	 */
	for (int T1p1 = 0; T1p1 < 8; T1p1++) {
		ts->tasks[0].phase_1_prio = T1p1;

		for (int T2p1 = T1p1 + 1; T2p1 < 8; T2p1++) {
			ts->tasks[1].phase_1_prio = T2p1;

			for (int T3p1 = T2p1 + 1; T3p1 < 8; T3p1++) {
				ts->tasks[2].phase_1_prio = T3p1;

				for (int T4p1 = T3p1 + 1; T4p1 < 8; T4p1++) {
					ts->tasks[3].phase_1_prio = T4p1;

					for (int T1p2 = 0; T1p2 < 8; T1p2++) {
						if (T1p2 == T1p1) continue;
						if (T1p2 == T2p1) continue;
						if (T1p2 == T3p1) continue;
						if (T1p2 == T4p1) continue;
						ts->tasks[0].phase_2_prio = T1p2;

						for (int T2p2 = 0; T2p2 < 8; T2p2++) {
							if (T2p2 == T1p1) continue;
							if (T2p2 == T2p1) continue;
							if (T2p2 == T3p1) continue;
							if (T2p2 == T4p1) continue;
							if (T2p2 == T1p2) continue;
							ts->tasks[1].phase_2_prio = T2p2;

							for (int T3p2 = 0; T3p2 < 8; T3p2++) {
								if (T3p2 == T1p1) continue;
								if (T3p2 == T2p1) continue;
								if (T3p2 == T3p1) continue;
								if (T3p2 == T4p1) continue;
								if (T3p2 == T1p2) continue;
								if (T3p2 == T2p2) continue;
								ts->tasks[2].phase_2_prio = T3p2;

								for (int T4p2 = 0; T4p2 < 8; T4p2++) {
									if (T4p2 == T1p1) continue;
									if (T4p2 == T2p1) continue;
									if (T4p2 == T3p1) continue;
									if (T4p2 == T4p1) continue;
									if (T4p2 == T1p2) continue;
									if (T4p2 == T2p2) continue;
									if (T4p2 == T3p2) continue;
									ts->tasks[3].phase_2_prio = T4p2;
									
									generated_permutations++;

									printf("Generated priority permutation "
										"%ld of %ld...\n",
										generated_permutations, 
										total_permutations);
									print_taskset(ts, 1, 0);
									
									/*
									 * Test all possible combinations of phase
									 * change points with these priorities by
									 * simulating the SAS.
									 */
									if (test_all_phase_change_points(ts)) {
										return 1; /* Return if schedulable */
									}
									printf("Unschedulable for all combinations "
										   "of phase change points.\n\n");
								}
							}
						}
					}
				}
			}
		}
	}
	/* Not schedulable with any priority permutation */
	assert(generated_permutations == total_permutations);
	printf("Task set is not dual-priority schedulable with RM for phase 1!\n");
	return 0; 
}

/*
 * ============================================================================
 * Functions for verifying the three counterexamples in the paper 
 * "Dual Priority Scheduling is Not Optimal".
 * ============================================================================
 */

/*
 * Verify the claim that dual priority scheduling is not an optimal scheduling
 * algorithm.
 *
 * Counterexample 8 in the paper "Dual Priority Scheduling is Not Optimal".
 *
 * Runtime: ~61h on an AMD Ryzen 7 1700X running Linux 4.20 when compiled
 *          by GCC 8.2.1 with -O3 optimization settings.
 *
 * The counterexample used is the following task set.
 *
 *    T1 = (8,  19)
 *    T2 = (13, 29)
 *    T3 = (9,  151)
 *    T4 = (14, 197)
 *    
 *    Hyper-period: 16390597
 *    Utilization:  16390550 / 16390597 ~= 0.9999971
 *
 * This function will assert that the there are no schedulable configurations
 * of priorities and phase change points for the above task set.
 *
 * It was originally conjectured in [1] that dual priority scheduling is
 * optimal for synchronous periodic tasks with implicit deadlines. This
 * counterexample demonstrates the falsity of the conjecture.
 *
 * [1] A. Burns and A.J. Wellings.
 *     "Dual Priority Assignment: A Practical Method for Increasing Processor
 *     Utilisation"
 *     In EMWRTS, 1993.
 */
void verify_counterexample_1() {
	struct task_t T1, T2, T3, T4;
	T1.wcet = 8;  T1.period = 19;
	T2.wcet = 13; T2.period = 29;
	T3.wcet = 9;  T3.period = 151;
	T4.wcet = 14; T4.period = 197;

	struct taskset_t ts;
	ts.tasks[0] = T1;
	ts.tasks[1] = T2;
	ts.tasks[2] = T3;
	ts.tasks[3] = T4;
	ts.hyper_period = hyper_period(&ts);

	printf("Running test 1...\n\n");

	printf("Exhaustively testing all configurations...\n\n");
	int schedulable = test_all_configurations_exhaustively(&ts);
	if (schedulable) { /* Will not happen */
		printf("\nTest 1 failed: task set is schedulable.\n");
		exit(EXIT_FAILURE);
	}

	printf("\nSuccessfully finished test 1.\n");
}

/*
 * Verify the claim that setting phase 1 priorities to Rate Monotonic (RM)
 * is not an optimal priority policy for dual priority scheduling.
 *
 * Counterexample 9 in the paper "Dual Priority Scheduling is Not Optimal".
 *
 * Runtime: ~13h on an AMD Ryzen 7 1700X running Linux 4.20 when compiled
 *          by GCC 8.2.1 with -O3 optimization settings.
 *
 * The counterexample used is the following task set.
 *
 *    T1 = (13, 29)
 *    T2 = (17, 47)
 *    T3 = (4,  89)
 *    T4 = (28, 193)
 *    
 *    Hyper-period: 23412251
 *    Utilization:  23412240 / 23412251 ~= 0.9999995
 *
 * This function will assert that the there are no schedulable configurations
 * of priorities and phase change points for the above task set when phase 1
 * priorities are RM. It will then assert that the following is a schedulable
 * configuration:
 *
 *    T1 phase 1 prio = 4, T1 phase 2 prio = 0, T1 phase change point = 13
 *    T2 phase 1 prio = 5, T2 phase 2 prio = 1, T2 phase change point = 17
 *    T3 phase 1 prio = 7, T3 phase 2 prio = 2, T3 phase change point = 42
 *    T4 phase 1 prio = 6, T4 phase 2 prio = 3, T4 phase change point = 139
 *
 * It was conjectured in [1] and [2] that setting phase 1 priorities to
 * RM is optimal for dual priority scheduling. In [3] and [4] it was even
 * conjectured that RM+RM priorities are optimal. This counterexample
 * demonstrates the falsity of these conjectures.
 *
 * [1] A. Burns and A.J. Wellings.
 *     "Dual Priority Assignment: A Practical Method for Increasing Processor
 *     Utilisation"
 *     In EMWRTS, 1993.
 *
 * [2] A. Burns.
 *     "Dual Priority Scheduling: Is the Utilisation Bound 100%?"
 *     In RTSOPS, 2010.
 *
 * [3] L. George, J. Goossens and D. Masson.
 *     "Dual Priority and EDF: a closer look"
 *     In WiP-RTSS, 2014.
 *
 * [4] T. Fautrel, L. George, J. Goossens, D. Masson and P. Rodriguez.
 *     "A Practical Sub-Optimal Solution for the Dual Priority Scheduling
 *     Problem"
 *     In SIES, 2018.
 */
void verify_counterexample_2() {
	struct task_t T1, T2, T3, T4;
	T1.wcet = 13; T1.period = 29;
	T2.wcet = 17; T2.period = 47;
	T3.wcet = 4;  T3.period = 89;
	T4.wcet = 28; T4.period = 193;

	struct taskset_t ts;
	ts.tasks[0] = T1;
	ts.tasks[1] = T2;
	ts.tasks[2] = T3;
	ts.tasks[3] = T4;
	ts.hyper_period = hyper_period(&ts);

	printf("Running test 2...\n\n");

	printf("Exhaustively testing all configurations with RM for phase 1 "
			"priorites...\n\n");
	int rm_schedulable = test_rm_configurations_exhaustively(&ts);
	if (rm_schedulable) { /* Will not happen */
		printf("\nTest 2 failed: task set schedulable with RM for phase 1.\n");
		exit(EXIT_FAILURE);
	}

	printf("\nTesting custom configuration...\n");
	ts.tasks[0].phase_1_prio = 4; ts.tasks[0].phase_2_prio = 0;
	ts.tasks[1].phase_1_prio = 5; ts.tasks[1].phase_2_prio = 1;
	ts.tasks[2].phase_1_prio = 7; ts.tasks[2].phase_2_prio = 2;
	ts.tasks[3].phase_1_prio = 6; ts.tasks[3].phase_2_prio = 3;
	ts.tasks[0].phase_change_point = 13;
	ts.tasks[1].phase_change_point = 17;
	ts.tasks[2].phase_change_point = 42;
	ts.tasks[3].phase_change_point = 139;
	print_taskset(&ts, 1, 1);
	if (simulate_sas(&ts) != NULL) { /* Will not happen */
		printf("\nTest 2 failed: custom configuration not schedulable.\n");
		exit(EXIT_FAILURE);
	}
	printf("Task set schedulable with custom configuration.\n");
 
	printf("\nSuccessfully finished test 2.\n");
}

/*
 * Verify the claim that the FDMS policy is not an optimal way of finding
 * phase change points with RM+RM priority ordering.
 *
 * Counterexample 10 in the paper "Dual Priority Scheduling is Not Optimal".
 *
 * Runtime: ~0.01s on an AMD Ryzen 7 1700X running Linux 4.20 when compiled
 *          by GCC 8.2.1 with -O3 optimization settings.
 *
 * The counterexample used is the following task set.
 *
 *    T1 = (6, 11)
 *    T2 = (6, 20)
 *    T3 = (4, 46)
 *    T4 = (5, 74)
 *    
 *    Hyper-period: 187220
 *    Utilization:  46804 / 46805 ~= 0.9999786
 *
 * This function will assert that the FDMS policy fails, and then assert that
 * the task set is in fact schedulable with RM+RM priorities using these
 * phase change points:
 *
 *    T1 phase change point = 5
 *    T2 phase change point = 3
 *    T3 phase change point = 25
 *    T4 phase change point = 35
 *
 * In [1] it was conjectured that the FDMS policy is an optimal way of finding
 * phase change points with RM+RM priorities. This counterexample shows that
 * the conjecture does not hold.
 *
 * [1] T. Fautrel, L. George, J. Goossens, D. Masson and P. Rodriguez.
 *     "A Practical Sub-Optimal Solution for the Dual Priority Scheduling
 *     Problem"
 *     In SIES, 2018.
 */
void verify_counterexample_3() {
	struct task_t T1, T2, T3, T4;
	T1.wcet = 6; T1.period = 11;
	T2.wcet = 6; T2.period = 20;
	T3.wcet = 4; T3.period = 46;
	T4.wcet = 5; T4.period = 74;

	struct taskset_t ts;
	ts.tasks[0] = T1;
	ts.tasks[1] = T2;
	ts.tasks[2] = T3;
	ts.tasks[3] = T4;
	ts.hyper_period = hyper_period(&ts);

	printf("Running test 3...\n\n");

	printf("Setting RM+RM priorities...\n\n");
	ts.tasks[0].phase_1_prio = 4; ts.tasks[0].phase_2_prio = 0;
	ts.tasks[1].phase_1_prio = 5; ts.tasks[1].phase_2_prio = 1;
	ts.tasks[2].phase_1_prio = 6; ts.tasks[2].phase_2_prio = 2;
	ts.tasks[3].phase_1_prio = 7; ts.tasks[3].phase_2_prio = 3;

	printf("Testing the FDMS policy for finding phase change points...\n");
	int fdms_schedulable = test_fdms_phase_change_points(&ts);
	if (fdms_schedulable) { /* Will not happen */
		printf("\nTest 3 failed: task set schedulable with FDMS.\n");
		exit(EXIT_FAILURE);
	}
	printf("Task set not schedulable with the FDMS policy.\n\n");

	printf("Testing custom RM+RM configuration...\n");
	ts.tasks[0].phase_change_point = 5;
	ts.tasks[1].phase_change_point = 3;
	ts.tasks[2].phase_change_point = 25;
	ts.tasks[3].phase_change_point = 35;
	print_taskset(&ts, 1, 1);
	if (simulate_sas(&ts) != NULL) { /* Will not happen */
		printf("\nTest 3 failed: custom configuration not schedulable.\n");
		exit(EXIT_FAILURE);
	}
	printf("Task set schedulable with custom configuration.\n");
	
	printf("\nSuccessfully finished test 3.\n");
}

void print_help_and_exit() {
	char *help = \
		"This program simulates dual priority scheduling of periodic tasks\n"
		"and verifies the counterexamples given in the paper entitled\n"
		"\"Dual Priority Scheduling is Not Optimal\".\n\n"
		"Usage: dualpriotest TEST_NUM\n\n"
		"where TEST_NUM is 1, 2, or 3.\n\n"
		"Test 1: Show the suboptimality of dual priority scheduling.\n"
		"        Counterexample 8 in the paper (very, very slow).\n\n"
		"Test 2: Show the suboptimality of RM ordering of phase 1 priorities\n"
		"        Counterexample 9 in the paper (very slow).\n\n"
		"Test 3: Show the suboptimality of FDMS phase change points\n"
		"        Counterexample 10 in the paper (fast).\n";
	printf("%s", help);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {

	if (argc != 2) {
		print_help_and_exit();
	}

	switch (atoi(argv[1])) {
		case 1:
			verify_counterexample_1();
			break;
		case 2:
			verify_counterexample_2();
			break;
		case 3:
			verify_counterexample_3();
			break;
		default:
			print_help_and_exit();
	}

	return EXIT_SUCCESS;
}

