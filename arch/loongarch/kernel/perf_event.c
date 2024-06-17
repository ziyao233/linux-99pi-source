// SPDX-License-Identifier: GPL-2.0
/*
 * Linux performance counter support for LoongArch.
 *
 * Copyright (C) 2020 Loongson Technology Corporation Limited
 *
 * Author: Huacai Chen <chenhuacai@loongson.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/smp.h>
#include <linux/kernel.h>
#include <linux/perf_event.h>
#include <linux/uaccess.h>
#include <linux/sched/task_stack.h>

#include <asm/irq.h>
#include <asm/irq_regs.h>
#include <asm/stacktrace.h>
#include <asm/time.h> /* For perf_irq */
#include <asm/unwind.h>

/*
 * Get the return address for a single stackframe and return a pointer to the
 * next frame tail.
 */
static unsigned long user_backtrace(struct perf_callchain_entry_ctx *entry,
							unsigned long fp)
{
	unsigned long err;
	unsigned long __user *user_frame_tail;
	struct stack_frame buftail;

	user_frame_tail = (unsigned long __user *)(fp - sizeof(struct stack_frame));

	/* Also check accessibility of one struct frame_tail beyond */
	if (!access_ok(VERIFY_READ, user_frame_tail, sizeof(buftail)))
		return 0;

	pagefault_disable();
	err = __copy_from_user_inatomic(&buftail, user_frame_tail, sizeof(buftail));
	pagefault_enable();

	if (err || (unsigned long)user_frame_tail >= buftail.fp)
		return 0;

	perf_callchain_store(entry, buftail.ra);

	return buftail.fp;
}

void perf_callchain_user(struct perf_callchain_entry_ctx *entry,
			 struct pt_regs *regs)
{
	unsigned long fp;

	if (perf_guest_cbs && perf_guest_cbs->is_in_guest()) {
		/* We don't support guest os callchain now */
		return;
	}

	perf_callchain_store(entry, regs->csr_era);

	fp = regs->regs[22];

	while (entry->nr < entry->max_stack && fp && !((unsigned long)fp & 0xf))
		fp = user_backtrace(entry, fp);
}

void perf_callchain_kernel(struct perf_callchain_entry_ctx *entry,
			   struct pt_regs *regs)
{
	unsigned long addr;
	struct unwind_state state;

	for (unwind_start(&state, current, regs);
	      !unwind_done(&state); unwind_next_frame(&state)) {
		addr = unwind_get_return_address(&state);
		if (!addr || perf_callchain_store(entry, addr))
			return;
	}
}

#define LOONGARCH_MAX_HWEVENTS 32

struct cpu_hw_events {
	/* Array of events on this cpu. */
	struct perf_event	*events[LOONGARCH_MAX_HWEVENTS];

	/*
	 * Set the bit (indexed by the counter number) when the counter
	 * is used for an event.
	 */
	unsigned long		used_mask[BITS_TO_LONGS(LOONGARCH_MAX_HWEVENTS)];

	/*
	 * Software copy of the control register for each performance counter.
	 * LoongArch CPUs vary in performance counters. They use this differently,
	 * and even may not use it.
	 */
	unsigned int		saved_ctrl[LOONGARCH_MAX_HWEVENTS];
};

static DEFINE_PER_CPU(struct cpu_hw_events, cpu_hw_events) = {
	.saved_ctrl = {0},
};

/* The description of LoongArch performance events. */
struct loongarch_perf_event {
	unsigned int event_id;
};

static struct loongarch_perf_event raw_event;
static DEFINE_MUTEX(raw_event_mutex);

#define C(x) PERF_COUNT_HW_CACHE_##x
#define HW_OP_UNSUPPORTED		0xffffffff
#define CACHE_OP_UNSUPPORTED		0xffffffff

#define PERF_MAP_ALL_UNSUPPORTED					\
	[0 ... PERF_COUNT_HW_MAX - 1] = {HW_OP_UNSUPPORTED}

#define PERF_CACHE_MAP_ALL_UNSUPPORTED					\
[0 ... C(MAX) - 1] = {							\
	[0 ... C(OP_MAX) - 1] = {					\
		[0 ... C(RESULT_MAX) - 1] = {CACHE_OP_UNSUPPORTED},	\
	},								\
}

struct loongarch_pmu {
	u64		max_period;
	u64		valid_count;
	u64		overflow;
	const char	*name;
	unsigned int	num_counters;
	u64		(*read_counter)(unsigned int idx);
	void		(*write_counter)(unsigned int idx, u64 val);
	const struct loongarch_perf_event *(*map_raw_event)(u64 config);
	const struct loongarch_perf_event (*general_event_map)[PERF_COUNT_HW_MAX];
	const struct loongarch_perf_event (*cache_event_map)
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX];
};

static struct loongarch_pmu loongarch_pmu;

#define M_PERFCTL_EVENT(event)	(event & CSR_PERFCTRL_EVENT)

#define M_PERFCTL_COUNT_EVENT_WHENEVER	(CSR_PERFCTRL_PLV0 |	\
					CSR_PERFCTRL_PLV1 |	\
					CSR_PERFCTRL_PLV2 |	\
					CSR_PERFCTRL_PLV3 |	\
					CSR_PERFCTRL_IE)

#define M_PERFCTL_CONFIG_MASK		0x1f0000

static void resume_local_counters(void);
static void pause_local_counters(void);

static u64 loongarch_pmu_read_counter(unsigned int idx)
{
	u64 val = -1;

	switch (idx) {
	case 0:
		val = read_csr_perfcntr0();
		break;
	case 1:
		val = read_csr_perfcntr1();
		break;
	case 2:
		val = read_csr_perfcntr2();
		break;
	case 3:
		val = read_csr_perfcntr3();
		break;
	default:
		WARN_ONCE(1, "Invalid performance counter number (%d)\n", idx);
		return 0;
	}

	return val;
}

static void loongarch_pmu_write_counter(unsigned int idx, u64 val)
{
	switch (idx) {
	case 0:
		write_csr_perfcntr0(val);
		return;
	case 1:
		write_csr_perfcntr1(val);
		return;
	case 2:
		write_csr_perfcntr2(val);
		return;
	case 3:
		write_csr_perfcntr3(val);
		return;
	}
}

static unsigned int loongarch_pmu_read_control(unsigned int idx)
{
	unsigned int val = -1;

	switch (idx) {
	case 0:
		val = read_csr_perfctrl0();
		break;
	case 1:
		val = read_csr_perfctrl1();
		break;
	case 2:
		val = read_csr_perfctrl2();
		break;
	case 3:
		val = read_csr_perfctrl3();
		break;
	default:
		WARN_ONCE(1, "Invalid performance counter number (%d)\n", idx);
		return 0;
	}

	return val;
}

static void loongarch_pmu_write_control(unsigned int idx, unsigned int val)
{
	switch (idx) {
	case 0:
		write_csr_perfctrl0(val);
		return;
	case 1:
		write_csr_perfctrl1(val);
		return;
	case 2:
		write_csr_perfctrl2(val);
		return;
	case 3:
		write_csr_perfctrl3(val);
		return;
	default:
		WARN_ONCE(1, "Invalid performance counter number (%d)\n", idx);
		return;
	}
}

static int loongarch_pmu_alloc_counter(struct cpu_hw_events *cpuc, struct hw_perf_event *hwc)
{
	int i;

	for (i = 0; i < loongarch_pmu.num_counters; i++) {
		if (!test_and_set_bit(i, cpuc->used_mask))
			return i;
	}

	return -EAGAIN;
}

static void loongarch_pmu_enable_event(struct hw_perf_event *evt, int idx)
{
	unsigned int cpu;
	struct perf_event *event = container_of(evt, struct perf_event, hw);
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);

	WARN_ON(idx < 0 || idx >= loongarch_pmu.num_counters);

	cpuc->saved_ctrl[idx] = M_PERFCTL_EVENT(evt->event_base) |
		(evt->config_base & M_PERFCTL_CONFIG_MASK) | CSR_PERFCTRL_IE;

	cpu = (event->cpu >= 0) ? event->cpu : smp_processor_id();

	/*
	 * We do not actually let the counter run. Leave it until start().
	 */
	pr_debug("Enabling perf counter for CPU%d\n", cpu);
}

static void loongarch_pmu_disable_event(int idx)
{
	unsigned long flags;
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);

	WARN_ON(idx < 0 || idx >= loongarch_pmu.num_counters);

	local_irq_save(flags);
	cpuc->saved_ctrl[idx] = loongarch_pmu_read_control(idx) &
		~M_PERFCTL_COUNT_EVENT_WHENEVER;
	loongarch_pmu_write_control(idx, cpuc->saved_ctrl[idx]);
	local_irq_restore(flags);
}

static int loongarch_pmu_event_set_period(struct perf_event *event,
				    struct hw_perf_event *hwc,
				    int idx)
{
	int ret = 0;
	u64 period = hwc->sample_period;
	u64 left = local64_read(&hwc->period_left);


	if (unlikely((left + period) & (1ULL << 63))) {
		/* left underflowed by more than period. */
		left = period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	} else	if (unlikely((left + period) <= period)) {
		/* left underflowed by less than period. */
		left += period;
		local64_set(&hwc->period_left, left);
		hwc->last_period = period;
		ret = 1;
	}

	if (left > loongarch_pmu.max_period) {
		left = loongarch_pmu.max_period;
		local64_set(&hwc->period_left, left);
	}

	local64_set(&hwc->prev_count, loongarch_pmu.overflow - left);

	loongarch_pmu.write_counter(idx, loongarch_pmu.overflow - left);

	perf_event_update_userpage(event);

	return ret;
}

static void loongarch_pmu_event_update(struct perf_event *event,
				 struct hw_perf_event *hwc,
				 int idx)
{
	u64 prev_raw_count, new_raw_count;
	u64 delta;

again:
	prev_raw_count = local64_read(&hwc->prev_count);
	new_raw_count = loongarch_pmu.read_counter(idx);

	if (local64_cmpxchg(&hwc->prev_count, prev_raw_count,
				new_raw_count) != prev_raw_count)
		goto again;

	delta = new_raw_count - prev_raw_count;

	local64_add(delta, &event->count);
	local64_sub(delta, &hwc->period_left);
}

static void loongarch_pmu_start(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (flags & PERF_EF_RELOAD)
		WARN_ON_ONCE(!(hwc->state & PERF_HES_UPTODATE));

	hwc->state = 0;

	/* Set the period for the event. */
	loongarch_pmu_event_set_period(event, hwc, hwc->idx);

	/* Enable the event. */
	loongarch_pmu_enable_event(hwc, hwc->idx);
}

static void loongarch_pmu_stop(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;

	if (!(hwc->state & PERF_HES_STOPPED)) {
		/* We are working on a local event. */
		loongarch_pmu_disable_event(hwc->idx);
		barrier();
		loongarch_pmu_event_update(event, hwc, hwc->idx);
		hwc->state |= PERF_HES_STOPPED | PERF_HES_UPTODATE;
	}
}

static int loongarch_pmu_add(struct perf_event *event, int flags)
{
	int idx, err = 0;
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	struct hw_perf_event *hwc = &event->hw;

	perf_pmu_disable(event->pmu);

	/* To look for a free counter for this event. */
	idx = loongarch_pmu_alloc_counter(cpuc, hwc);
	if (idx < 0) {
		err = idx;
		goto out;
	}

	/*
	 * If there is an event in the counter we are going to use then
	 * make sure it is disabled.
	 */
	event->hw.idx = idx;
	loongarch_pmu_disable_event(idx);
	cpuc->events[idx] = event;

	hwc->state = PERF_HES_STOPPED | PERF_HES_UPTODATE;
	if (flags & PERF_EF_START)
		loongarch_pmu_start(event, PERF_EF_RELOAD);

	/* Propagate our changes to the userspace mapping. */
	perf_event_update_userpage(event);

out:
	perf_pmu_enable(event->pmu);
	return err;
}

static void loongarch_pmu_del(struct perf_event *event, int flags)
{
	struct hw_perf_event *hwc = &event->hw;
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);
	int idx = hwc->idx;

	WARN_ON(idx < 0 || idx >= loongarch_pmu.num_counters);

	loongarch_pmu_stop(event, PERF_EF_UPDATE);
	cpuc->events[idx] = NULL;
	clear_bit(idx, cpuc->used_mask);

	perf_event_update_userpage(event);
}

static void loongarch_pmu_read(struct perf_event *event)
{
	struct hw_perf_event *hwc = &event->hw;

	/* Don't read disabled counters! */
	if (hwc->idx < 0)
		return;

	loongarch_pmu_event_update(event, hwc, hwc->idx);
}

static void loongarch_pmu_enable(struct pmu *pmu)
{
	resume_local_counters();
}

static void loongarch_pmu_disable(struct pmu *pmu)
{
	pause_local_counters();
}

static atomic_t active_events = ATOMIC_INIT(0);
static DEFINE_MUTEX(pmu_reserve_mutex);

static void reset_counters(void *arg);
static int __hw_perf_event_init(struct perf_event *event);

static void hw_perf_event_destroy(struct perf_event *event)
{
	if (atomic_dec_and_mutex_lock(&active_events,
				&pmu_reserve_mutex)) {
		/*
		 * We must not call the destroy function with interrupts
		 * disabled.
		 */
		on_each_cpu(reset_counters, NULL, 1);
		mutex_unlock(&pmu_reserve_mutex);
	}
}

static int loongarch_pmu_event_init(struct perf_event *event)
{
	/* does not support taken branch sampling */
	if (has_branch_stack(event))
		return -EOPNOTSUPP;

	switch (event->attr.type) {
	case PERF_TYPE_RAW:
	case PERF_TYPE_HARDWARE:
	case PERF_TYPE_HW_CACHE:
		break;

	default:
		/* Init it to avoid false validate_group */
		event->hw.event_base = 0xffffffff;
		return -ENOENT;
	}

	if (event->cpu >= 0 && !cpu_online(event->cpu))
		return -ENODEV;

	if (!atomic_inc_not_zero(&active_events)) {
		mutex_lock(&pmu_reserve_mutex);
		atomic_inc(&active_events);
		mutex_unlock(&pmu_reserve_mutex);
	}

	return __hw_perf_event_init(event);
}

static struct pmu pmu = {
	.pmu_enable	= loongarch_pmu_enable,
	.pmu_disable	= loongarch_pmu_disable,
	.event_init	= loongarch_pmu_event_init,
	.add		= loongarch_pmu_add,
	.del		= loongarch_pmu_del,
	.start		= loongarch_pmu_start,
	.stop		= loongarch_pmu_stop,
	.read		= loongarch_pmu_read,
};

static unsigned int loongarch_pmu_perf_event_encode(const struct loongarch_perf_event *pev)
{
	return M_PERFCTL_EVENT(pev->event_id);
}

static const struct loongarch_perf_event *loongarch_pmu_map_general_event(int idx)
{
	const struct loongarch_perf_event *pev;

	pev = &(*loongarch_pmu.general_event_map)[idx];

	if (pev->event_id == HW_OP_UNSUPPORTED)
		return ERR_PTR(-ENOENT);

	return pev;
}

static const struct loongarch_perf_event *loongarch_pmu_map_cache_event(u64 config)
{
	unsigned int cache_type, cache_op, cache_result;
	const struct loongarch_perf_event *pev;

	cache_type = (config >> 0) & 0xff;
	if (cache_type >= PERF_COUNT_HW_CACHE_MAX)
		return ERR_PTR(-EINVAL);

	cache_op = (config >> 8) & 0xff;
	if (cache_op >= PERF_COUNT_HW_CACHE_OP_MAX)
		return ERR_PTR(-EINVAL);

	cache_result = (config >> 16) & 0xff;
	if (cache_result >= PERF_COUNT_HW_CACHE_RESULT_MAX)
		return ERR_PTR(-EINVAL);

	pev = &((*loongarch_pmu.cache_event_map)
					[cache_type]
					[cache_op]
					[cache_result]);

	if (pev->event_id == CACHE_OP_UNSUPPORTED)
		return ERR_PTR(-ENOENT);

	return pev;
}

static int validate_group(struct perf_event *event)
{
	struct cpu_hw_events fake_cpuc;
	struct perf_event *sibling, *leader = event->group_leader;

	memset(&fake_cpuc, 0, sizeof(fake_cpuc));

	if (loongarch_pmu_alloc_counter(&fake_cpuc, &leader->hw) < 0)
		return -EINVAL;

	for_each_sibling_event(sibling, leader) {
		if (loongarch_pmu_alloc_counter(&fake_cpuc, &sibling->hw) < 0)
			return -EINVAL;
	}

	if (loongarch_pmu_alloc_counter(&fake_cpuc, &event->hw) < 0)
		return -EINVAL;

	return 0;
}

/* This is needed by specific irq handlers in perf_event_*.c */
static void handle_associated_event(struct cpu_hw_events *cpuc,
				    int idx, struct perf_sample_data *data,
				    struct pt_regs *regs)
{
	struct perf_event *event = cpuc->events[idx];
	struct hw_perf_event *hwc = &event->hw;

	loongarch_pmu_event_update(event, hwc, idx);
	data->period = event->hw.last_period;
	if (!loongarch_pmu_event_set_period(event, hwc, idx))
		return;

	if (perf_event_overflow(event, data, regs))
		loongarch_pmu_disable_event(idx);
}

static void reset_counters(void *arg)
{
	int n;
	int counters = loongarch_pmu.num_counters;

	for (n = 0; n < counters; n++) {
		loongarch_pmu_write_control(n, 0);
		loongarch_pmu.write_counter(n, 0);
	}
}

static const struct loongarch_perf_event loongson_new_event_map[PERF_COUNT_HW_MAX] = {
	PERF_MAP_ALL_UNSUPPORTED,
	[PERF_COUNT_HW_CPU_CYCLES] = { 0x00 },
	[PERF_COUNT_HW_INSTRUCTIONS] = { 0x01 },
	[PERF_COUNT_HW_CACHE_REFERENCES] = { 0x08 },
	[PERF_COUNT_HW_CACHE_MISSES] = { 0x09 },
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS] = { 0x02 },
	[PERF_COUNT_HW_BRANCH_MISSES] = { 0x03 },
	[PERF_COUNT_HW_BUS_CYCLES] = { HW_OP_UNSUPPORTED },
	[PERF_COUNT_HW_STALLED_CYCLES_FRONTEND] = { HW_OP_UNSUPPORTED },
	[PERF_COUNT_HW_STALLED_CYCLES_BACKEND] = { HW_OP_UNSUPPORTED },
	[PERF_COUNT_HW_REF_CPU_CYCLES] = { HW_OP_UNSUPPORTED },
};

static const struct loongarch_perf_event loongson_new_cache_map
				[PERF_COUNT_HW_CACHE_MAX]
				[PERF_COUNT_HW_CACHE_OP_MAX]
				[PERF_COUNT_HW_CACHE_RESULT_MAX] = {
PERF_CACHE_MAP_ALL_UNSUPPORTED,
[C(L1D)] = {
	/*
	 * Like some other architectures (e.g. ARM), the performance
	 * counters don't differentiate between read and write
	 * accesses/misses, so this isn't strictly correct, but it's the
	 * best we can do. Writes and reads get combined.
	 */
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { 0x8 },
		[C(RESULT_MISS)]	= { 0x9 },
	},
	[C(OP_WRITE)] = {
		[C(RESULT_ACCESS)]	= { 0x8 },
		[C(RESULT_MISS)]	= { 0x9 },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { 0xaa },
		[C(RESULT_MISS)]	= { 0xa9 },
	},
},
[C(L1I)] = {
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { 0x6 },
		[C(RESULT_MISS)]	= { 0x7 },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { CACHE_OP_UNSUPPORTED },
		[C(RESULT_MISS)]	= { CACHE_OP_UNSUPPORTED },
	},

},
[C(LL)] = {
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { 0xc },
		[C(RESULT_MISS)]	= { 0xd },
	},
	[C(OP_WRITE)] = {
		[C(RESULT_ACCESS)]	= { 0xc },
		[C(RESULT_MISS)]	= { 0xd },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { CACHE_OP_UNSUPPORTED },
		[C(RESULT_MISS)]	= { CACHE_OP_UNSUPPORTED },
	},
},
[C(ITLB)] = {
	[C(OP_READ)] = {
		[C(RESULT_MISS)]    = { 0x3b },
		[C(RESULT_ACCESS)]	= { CACHE_OP_UNSUPPORTED },
	},
},
[C(DTLB)] = {
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { 0x4 },
		[C(RESULT_MISS)]	= { 0x3c },
	},
	[C(OP_WRITE)] = {
		[C(RESULT_ACCESS)]	= { 0x4 },
		[C(RESULT_MISS)]	= { 0x3c },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { CACHE_OP_UNSUPPORTED },
		[C(RESULT_MISS)]	= { CACHE_OP_UNSUPPORTED },
	},
},
[C(BPU)] = {
	/* Using the same code for *HW_BRANCH* */
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]  = { 0x02 },
		[C(RESULT_MISS)]    = { 0x03 },
	},
},
[C(NODE)] = {
	[C(OP_READ)] = {
		[C(RESULT_ACCESS)]	= { CACHE_OP_UNSUPPORTED },
		[C(RESULT_MISS)]	= { CACHE_OP_UNSUPPORTED },
	},
	[C(OP_WRITE)] = {
		[C(RESULT_ACCESS)]	= { CACHE_OP_UNSUPPORTED },
		[C(RESULT_MISS)]	= { CACHE_OP_UNSUPPORTED },
	},
	[C(OP_PREFETCH)] = {
		[C(RESULT_ACCESS)]	= { CACHE_OP_UNSUPPORTED },
		[C(RESULT_MISS)]	= { CACHE_OP_UNSUPPORTED },
	},
},
};

static int __hw_perf_event_init(struct perf_event *event)
{
	int err;
	struct hw_perf_event *hwc = &event->hw;
	const struct loongarch_perf_event *pev;
	struct perf_event_attr *attr = &event->attr;

	/* Returning LoongArch event descriptor for generic perf event. */
	if (PERF_TYPE_HARDWARE == event->attr.type) {
		if (event->attr.config >= PERF_COUNT_HW_MAX)
			return -EINVAL;
		pev = loongarch_pmu_map_general_event(event->attr.config);
	} else if (PERF_TYPE_HW_CACHE == event->attr.type) {
		pev = loongarch_pmu_map_cache_event(event->attr.config);
	} else if (PERF_TYPE_RAW == event->attr.type) {
		/* We are working on the global raw event. */
		mutex_lock(&raw_event_mutex);
		pev = loongarch_pmu.map_raw_event(event->attr.config);
	} else {
		/* The event type is not (yet) supported. */
		return -EOPNOTSUPP;
	}

	if (IS_ERR(pev)) {
		if (PERF_TYPE_RAW == event->attr.type)
			mutex_unlock(&raw_event_mutex);
		return PTR_ERR(pev);
	}

	/*
	 * We allow max flexibility on how each individual counter shared
	 * by the single CPU operates (the mode exclusion and the range).
	 */
	hwc->config_base = CSR_PERFCTRL_IE;

	hwc->event_base = loongarch_pmu_perf_event_encode(pev);
	if (PERF_TYPE_RAW == event->attr.type)
		mutex_unlock(&raw_event_mutex);

	if (!attr->exclude_user) {
		hwc->config_base |= CSR_PERFCTRL_PLV3;
		hwc->config_base |= CSR_PERFCTRL_PLV2;
	}
	if (!attr->exclude_kernel) {
		hwc->config_base |= CSR_PERFCTRL_PLV0;
	}
	if (!attr->exclude_hv) {
		hwc->config_base |= CSR_PERFCTRL_PLV1;
	}

	hwc->config_base &= M_PERFCTL_CONFIG_MASK;
	/*
	 * The event can belong to another cpu. We do not assign a local
	 * counter for it for now.
	 */
	hwc->idx = -1;
	hwc->config = 0;

	if (!hwc->sample_period) {
		hwc->sample_period  = loongarch_pmu.max_period;
		hwc->last_period    = hwc->sample_period;
		local64_set(&hwc->period_left, hwc->sample_period);
	}

	err = 0;
	if (event->group_leader != event)
		err = validate_group(event);

	event->destroy = hw_perf_event_destroy;

	if (err)
		event->destroy(event);

	return err;
}

static void pause_local_counters(void)
{
	unsigned long flags;
	int ctr = loongarch_pmu.num_counters;
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);

	local_irq_save(flags);
	do {
		ctr--;
		cpuc->saved_ctrl[ctr] = loongarch_pmu_read_control(ctr);
		loongarch_pmu_write_control(ctr, cpuc->saved_ctrl[ctr] &
					 ~M_PERFCTL_COUNT_EVENT_WHENEVER);
	} while (ctr > 0);
	local_irq_restore(flags);
}

static void resume_local_counters(void)
{
	int ctr = loongarch_pmu.num_counters;
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);


	do {
		ctr--;
		loongarch_pmu_write_control(ctr, cpuc->saved_ctrl[ctr]);
	} while (ctr > 0);
}

void pmu_handle_irq(int irq)
{
	int handled = IRQ_NONE;
	unsigned int counters = loongarch_pmu.num_counters;
	u64 counter;
	struct pt_regs *regs;
	struct perf_sample_data data;
	struct cpu_hw_events *cpuc = this_cpu_ptr(&cpu_hw_events);

	/*
	 * First we pause the local counters, so that when we are locked
	 * here, the counters are all paused. When it gets locked due to
	 * perf_disable(), the timer interrupt handler will be delayed.
	 *
	 * See also loongarch_pmu_start().
	 */
	pause_local_counters();

	regs = get_irq_regs();

	perf_sample_data_init(&data, 0, 0);

	switch (counters) {
#define HANDLE_COUNTER(n)						\
	case n + 1:							\
		if (test_bit(n, cpuc->used_mask)) {			\
			counter = loongarch_pmu.read_counter(n);		\
			if (counter & loongarch_pmu.overflow) {		\
				handle_associated_event(cpuc, n, &data, regs); \
				handled = IRQ_HANDLED;			\
			}						\
		}
	HANDLE_COUNTER(3)
	HANDLE_COUNTER(2)
	HANDLE_COUNTER(1)
	HANDLE_COUNTER(0)
	}

	resume_local_counters();

	/*
	 * Do all the work for the pending perf events. We can do this
	 * in here because the performance counter interrupt is a regular
	 * interrupt, not NMI.
	 */
	if (handled == IRQ_HANDLED)
		irq_work_run();
}

static const struct loongarch_perf_event *loongarch_pmu_map_raw_event(u64 config)
{
	/* currently most cores have 10-bit event numbers */
	unsigned int raw_id = M_PERFCTL_EVENT(config);
	unsigned int base_id = M_PERFCTL_EVENT(raw_id);

	switch (current_cpu_data.cputype) {
	case CPU_LOONGSON64:
		base_id = raw_id;
		break;
	default:
		panic("Unkown cpu type '0x%x\n", current_cpu_data.cputype);
	}

	raw_event.event_id = base_id;

	return &raw_event;
}

static int __init init_hw_perf_events(void)
{
	int counters;

	if (!cpu_has_pmp)
		return -ENODEV;

	pr_info("Performance counters: ");
	counters = ((read_cpucfg(LOONGARCH_CPUCFG6) & CPUCFG6_PMNUM) >> 4) + 1;

	if (counters == 0) {
		pr_cont("No available PMU.\n");
		return -ENODEV;
	}

	loongarch_pmu.map_raw_event = loongarch_pmu_map_raw_event;

	switch (current_cpu_data.cputype) {
	case CPU_LOONGSON64:
		loongarch_pmu.name = "loongarch/loongson64";
		loongarch_pmu.general_event_map = &loongson_new_event_map;
		loongarch_pmu.cache_event_map = &loongson_new_cache_map;
		break;
	default:
		pr_cont("Either hardware does not support performance "
			"counters, or not yet implemented.\n");
		return -ENODEV;
	}

	loongarch_pmu.num_counters = counters;
	loongarch_pmu.max_period = (1ULL << 63) - 1;
	loongarch_pmu.valid_count = (1ULL << 63) - 1;
	loongarch_pmu.overflow = 1ULL << 63;
	loongarch_pmu.read_counter = loongarch_pmu_read_counter;
	loongarch_pmu.write_counter = loongarch_pmu_write_counter;

	on_each_cpu(reset_counters, NULL, 1);

	pr_cont("%s PMU enabled, %d %d-bit counters available to each "
		"CPU.\n", loongarch_pmu.name, counters, 64);

	perf_pmu_register(&pmu, "cpu", PERF_TYPE_RAW);

	return 0;
}
early_initcall(init_hw_perf_events);
