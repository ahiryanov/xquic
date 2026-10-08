/* Copyright (c) 2026, ahiryanov */
#include <CUnit/CUnit.h>
#include <limits.h>
#include <string.h>
#include "xqc_bbr2_test.h"
#include "src/congestion_control/xqc_bbr2.h"
#include "src/congestion_control/xqc_sample.h"
#include "src/common/xqc_log.h"
#include "src/transport/xqc_conn.h"
#include "src/transport/xqc_send_ctl.h"

#ifdef XQC_ENABLE_BBR2
typedef struct {
    xqc_bbr2_t bbr;
    xqc_sample_t sample;
    xqc_send_ctl_t ctl;
    xqc_connection_t conn;
    xqc_log_t log;
} xqc_bbr2_fixture_t;

static void xqc_bbr2_fixture_init(xqc_bbr2_fixture_t *f, uint64_t rtt,
    unsigned flags);

static void
xqc_bbr2_fixture_init(xqc_bbr2_fixture_t *f, uint64_t rtt, unsigned flags)
{
    memset(f, 0, sizeof(*f));
    f->conn.log = &f->log;
    f->ctl.ctl_conn = &f->conn;
    f->sample.send_ctl = &f->ctl;
    f->sample.srtt = rtt;
    f->sample.rtt = rtt;
    xqc_cc_params_t params = {.cc_optimization_flags = flags};
    xqc_bbr2_cb.xqc_cong_ctl_init_bbr(&f->bbr, &f->sample, params);
    f->bbr.send_ctl = &f->ctl;
    f->bbr.mode = BBR2_DRAIN;
    f->bbr.full_bandwidth_reached = 1;
    /* Keep the independent Reno trigger above the initial 0-1 rounds. */
    f->bbr.bw_hi[0] = rtt <= 200000 ? 100000000 : 1000000;
    f->sample.now = f->bbr.min_rtt_stamp + 1;
    xqc_bbr2_cb.xqc_cong_ctl_on_ack_multiple_pkts(&f->bbr, &f->sample);
}
#endif

void
xqc_test_bbr2_probe_wait(void)
{
#if defined(XQC_ENABLE_BBR2) && XQC_BBR2_PLUS_ENABLED
    const uint64_t rtts[] = {1000, 50000, 200000};
    xqc_bbr2_fixture_t f;
    for (unsigned i = 0; i < sizeof(rtts) / sizeof(rtts[0]); i++) {
        for (unsigned j = 0; j < 16; j++) {
            xqc_bbr2_fixture_init(&f, rtts[i],
                                 XQC_BBR2_FLAG_FAST_CONVERGENCE);
            uint32_t wait = f.bbr.probe_wait_us;
            CU_ASSERT(wait >= 5 * rtts[i] && wait <= 8 * rtts[i]);
            CU_ASSERT_EQUAL(wait % rtts[i], 0);
            CU_ASSERT_EQUAL(f.bbr.mode, BBR2_PROBE_BW);
            CU_ASSERT_EQUAL(f.bbr.cycle_idx, BBR2_BW_PROBE_CRUISE);
            uint64_t start = f.bbr.cycle_start_stamp;
            f.sample.now = start + 1;
            xqc_bbr2_cb.xqc_cong_ctl_on_ack_multiple_pkts(&f.bbr, &f.sample);
            CU_ASSERT_EQUAL(f.bbr.cycle_idx, BBR2_BW_PROBE_CRUISE);
            f.sample.now = start + wait;
            xqc_bbr2_cb.xqc_cong_ctl_on_ack_multiple_pkts(&f.bbr, &f.sample);
            CU_ASSERT_EQUAL(f.bbr.cycle_idx, BBR2_BW_PROBE_CRUISE);
            f.sample.now++;
            xqc_bbr2_cb.xqc_cong_ctl_on_ack_multiple_pkts(&f.bbr, &f.sample);
            CU_ASSERT_EQUAL(f.bbr.cycle_idx, BBR2_BW_PROBE_REFILL);
        }
    }
#endif
}

void
xqc_test_bbr2_probe_wait_boundaries(void)
{
#ifdef XQC_ENABLE_BBR2
    xqc_bbr2_fixture_t f;
    for (unsigned i = 0; i < 16; i++) {
        xqc_bbr2_fixture_init(&f, 50000, 0);
        CU_ASSERT(f.bbr.probe_wait_us >= 2000000);
        CU_ASSERT(f.bbr.probe_wait_us < 3000000);
    }
#if XQC_BBR2_PLUS_ENABLED
    const uint64_t invalid[] = {0, 0x7fffffff};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        xqc_bbr2_fixture_init(&f, invalid[i],
                             XQC_BBR2_FLAG_FAST_CONVERGENCE);
        CU_ASSERT(f.bbr.probe_wait_us >= 2000000);
        CU_ASSERT(f.bbr.probe_wait_us < 3000000);
    }
    xqc_bbr2_fixture_init(&f, 1000000000,
                         XQC_BBR2_FLAG_FAST_CONVERGENCE);
    CU_ASSERT_EQUAL(f.bbr.probe_wait_us, UINT32_MAX);
#endif
#endif
}

void
xqc_test_bbr2_probe_wait_reno(void)
{
#if defined(XQC_ENABLE_BBR2) && XQC_BBR2_PLUS_ENABLED
    xqc_bbr2_fixture_t f;
    xqc_bbr2_fixture_init(&f, 50000, XQC_BBR2_FLAG_FAST_CONVERGENCE);
    f.bbr.rounds_since_probe = 63;
    f.sample.now = f.bbr.cycle_start_stamp + 1;
    xqc_bbr2_cb.xqc_cong_ctl_on_ack_multiple_pkts(&f.bbr, &f.sample);
    CU_ASSERT_EQUAL(f.bbr.cycle_idx, BBR2_BW_PROBE_REFILL);
#endif
}

void
xqc_test_bbr2_probe_wait_idle(void)
{
#if defined(XQC_ENABLE_BBR2) && XQC_BBR2_PLUS_ENABLED
    xqc_bbr2_fixture_t f;
    xqc_bbr2_fixture_init(&f, 50000, XQC_BBR2_FLAG_FAST_CONVERGENCE);
    f.bbr.mode = BBR2_PROBE_RTT;
    f.bbr.probe_rtt_round_done_stamp = 1;
    /* Idle restart builds a synthetic sample without a send_ctl pointer. */
    xqc_bbr2_cb.xqc_cong_ctl_restart_from_idle(&f.bbr, 0);
    CU_ASSERT_EQUAL(f.bbr.mode, BBR2_PROBE_BW);
    CU_ASSERT_EQUAL(f.bbr.cycle_idx, BBR2_BW_PROBE_CRUISE);
    CU_ASSERT(f.bbr.probe_wait_us >= 250000);
    CU_ASSERT(f.bbr.probe_wait_us <= 400000);
#endif
}
