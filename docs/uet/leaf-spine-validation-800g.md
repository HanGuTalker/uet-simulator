# 800 Gbps leaf-spine validation

## Message-size sensitivity

The recommended 96/160 KiB RED thresholds were validated with 1, 4, and 16 MiB
All-to-All messages on the eight-node, 4:1 oversubscribed leaf-spine fabric.

| Message size | Completion | Makespan | Aggregate goodput | Marks | Drops / timeouts / retransmits | Jain fairness |
|---|---:|---:|---:|---:|---:|---:|
| 1 MiB | 56/56 | 288.925 us | 1625.896 Gbps | 607 | 0 / 0 / 0 | 0.993838 |
| 4 MiB | 56/56 | 1027.660 us | 1828.473 Gbps | 1123 | 0 / 0 / 0 | 0.998473 |
| 16 MiB | 56/56 | 3791.802 us | 1982.222 Gbps | 4027 | 0 / 0 / 0 | 0.999767 |

All three sizes complete without packet loss or recovery. Goodput and fairness
increase with message size as fixed startup costs are amortized and NSCC has
more time to converge. The short-message result shows that tail latency and
short-flow fairness should be evaluated separately from steady-state throughput.

## Ring rank-placement sensitivity

Two 4 MiB-per-rank Ring AllReduce operations were compared. Contiguous placement
uses ring order `0,1,2,3,4,5,6,7`; only two ring edges cross between leaves.
Interleaved placement uses `0,4,1,5,2,6,3,7`; every ring edge crosses a leaf-spine
uplink, so four flows contend on each 800 Gbps uplink during every step.

| Placement | Collective times | Aggregate wire goodput | Marks | Peak queue | Loss/recovery |
|---|---:|---:|---:|---:|---:|
| Contiguous | 372.365 / 233.366 us | 1551.058 Gbps | 0 | 4,184 B | none |
| Interleaved | 573.814 / 615.888 us | 789.714 Gbps | 38 | 220,064 B | none |

The interleaved mapping cuts aggregate wire goodput by about 49.1% and roughly
doubles collective completion time. ECN/NSCC keeps the congested ring lossless,
but cannot replace topology-aware rank ordering. This validates both the
congestion-control response and the importance of collective placement.

## Reproduction

Run `scripts/uec/run_leaf_spine_validation_800g.sh [output-directory]`. The
16 MiB case records per-packet queue and throughput traces and therefore takes
substantially longer than the other cases.
