# Work-conserving job scheduling at 800 Gbps

## Configuration

The experiment combines two interleaved 4 MiB, eight-rank Ring AllReduce
collectives with one 1 MiB-per-pair All-to-All background group. It uses the
two-leaf, one-spine fabric with 4:1 uplink oversubscription, 800 Gbps links,
RED/ECN thresholds of 96/160 KiB, and a 512 KiB queue limit.

Each endpoint uses smooth weighted round robin (SWRR) across active jobs. The
scheduler serializes packets at the endpoint's physical line rate, observes
each PDC's NSCC congestion window and pacing eligibility, and immediately lends
service to another job when the preferred job cannot send. PDCs within a job
are selected round robin. At mixed-workload start, the Ring job's aggregate
window is reset to its equal-job startup budget so that warm-up state does not
bypass admission control.

Isolated references are 573.814/615.888 us for the two Ring collectives and
309.312 us for the All-to-All group.

## Results

| Ring/background weight | Ring times | Ring slowdown | Background | Background slowdown | Aggregate goodput | ECN marks | Peak queue | Loss/recovery |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 1:1 | 888.717 / 700.544 us | 54.9% / 13.7% | 357.870 us | 15.7% | 886.756 Gbps | 144 | 266,344 B | none |
| 2:1 | 853.093 / 630.133 us | 48.7% / 2.3% | 375.207 us | 21.3% | 950.149 Gbps | 153 | 257,720 B | none |
| 4:1 | 907.864 / 641.277 us | 58.2% / 4.1% | 412.775 us | 33.4% | 909.721 Gbps | 437 | 372,888 B | none |

All three runs complete every message without queue drops, retransmissions,
timeouts, or NACKs. The aggregate goodput can exceed one link's 800 Gbps
because it sums delivered application bytes across the whole multi-link
fabric; it is not the utilization of a single physical link.

The 2:1 policy is the throughput-optimal tested point and gives the best first
and second Ring completion, while 1:1 gives the best background completion.
Raising the Ring weight to 4:1 is counterproductive: synchronized Ring traffic
produces more ECN feedback and a higher peak queue, so priority does not
translate into lower first-collective latency. No tested SWRR weight meets the
previously selected 20% Ring-slowdown target for the first collective, although
all meet the 50% background-slowdown target.

The first Ring collective remains sensitive to synchronized job startup. A
follow-up sweep of small All-to-All start offsets is documented in
`docs/uet/background-offset-sweep-800g.md`.

## Reproduction

Build the optimized ns-3 tree and run
`scripts/uec/run_work_conserving_scheduler_800g.sh [output-directory]`.
The final validated matrix is stored in
`results/work-conserving-scheduler-800g-final`.
