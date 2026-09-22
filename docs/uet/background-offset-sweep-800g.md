# Background-start offset sweep at 800 Gbps

## Purpose and configuration

This experiment tests whether a small admission offset can reduce the
synchronized startup burst in the mixed Ring AllReduce plus All-to-All
workload. It retains the selected 2:1 Ring/background smooth weighted
round-robin scheduler and all topology, NSCC, RED/ECN, and payload settings
from the work-conserving scheduler experiment. Only the background All-to-All
submission time changes.

Background FCT is measured from that background job's own submission time.
`Background finish` is measured from the Ring start and therefore includes the
intentional admission delay.

## Results

| Background offset | Ring 1 | Ring 2 | Background FCT | Background finish | Aggregate goodput | ECN marks | Peak queue | Loss/recovery |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0 us | 853.093 us | 630.133 us | 375.207 us | 375.207 us | 950.149 Gbps | 153 | 257,720 B | none |
| 10 us | 849.310 us | 621.286 us | 363.510 us | 373.510 us | 958.303 Gbps | 147 | 256,376 B | none |
| 20 us | 902.226 us | 691.697 us | 385.707 us | 405.707 us | 884.163 Gbps | 288 | 343,664 B | none |
| 25 us | 811.354 us | 697.867 us | 351.635 us | 376.635 us | 933.784 Gbps | 143 | 321,312 B | none |
| 30 us | 854.606 us | 626.912 us | 356.045 us | 386.045 us | 951.243 Gbps | 179 | 276,592 B | none |
| 50 us | 888.703 us | 674.230 us | 389.267 us | 439.267 us | 901.693 Gbps | 205 | 303,680 B | none |
| 100 us | 839.621 us | 631.615 us | 404.905 us | 504.905 us | 957.893 Gbps | 371 | 418,056 B | none |

All seven runs complete every message without drops, retransmissions,
timeouts, or NACKs.

The 10 us offset is the recommended balanced operating point. Relative to
simultaneous start, it improves both Ring collectives, improves background FCT
by 3.1%, completes the background 1.7 us earlier even after including the
delay, raises aggregate goodput by 0.9%, and slightly reduces ECN pressure.

The 25 us point is useful only when first-collective latency is the primary
objective: Ring 1 improves by 4.9% relative to simultaneous start, and its
background FCT is lowest, but Ring 2 is 10.7% slower than at zero offset. The
non-monotonic response comes from the phase relationship between Ring steps,
ACK/ECN feedback, and the delayed fan-out burst; a larger offset is not
inherently safer.

Admission timing improves the tradeoff but does not solve the first-collective
SLO: even the best Ring-1 point (25 us) is 41.4% slower than the isolated
573.814 us reference. The next model improvement should therefore target burst
shaping inside each job (paced release of simultaneously eligible PDCs), not a
larger fixed admission delay.

## Reproduction

Run `scripts/uec/run_background_offset_sweep_800g.sh [output-directory]` after
building the optimized ns-3 tree. The script is resumable and skips cases with
an existing summary JSON. The validated outputs are in
`results/background-offset-sweep-800g-final`.
