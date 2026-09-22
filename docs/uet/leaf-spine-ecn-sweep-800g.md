# 8-node 800 Gbps leaf-spine ECN sweep

## Topology and workload

Eight endpoints are divided evenly across two leaf switches. Every endpoint
link and both leaf-spine links run at 800 Gbps, but four endpoints on each leaf
share one 800 Gbps uplink. This creates 4:1 physical oversubscription without
changing the link rate used for comparison with the Falcon simulation.

The workload is one 4 MiB message for every directed endpoint pair (56 total),
with one 1 MiB warm-up per PDC. ECN cases use a 512 KiB RED limit. The no-ECN
baseline uses a 125-packet DropTail device queue, approximately the same byte
capacity.

Queue trace labels 1-8 identify leaf-to-endpoint egresses, 1001-1002 identify
leaf-to-spine uplinks, and 2001-2002 identify spine-to-leaf downlinks.

## Results

| Queue policy | Makespan | Aggregate goodput | Mean / p99 latency | Marks | Queue drops | Timeouts / retransmits | Jain fairness |
|---|---:|---:|---:|---:|---:|---:|---:|
| DropTail, ECN off | 912.770 us | 2058.622 Gbps | 469.671 / 912.770 us | 0 | 0 | 38 / 0 | 0.991216 |
| RED 64/96 KiB | 1090.283 us | 1723.450 Gbps | 675.014 / 1090.283 us | 2379 | 2 | 2 / 4 | 0.998882 |
| RED 96/160 KiB | 1027.660 us | 1828.473 Gbps | 607.455 / 1027.660 us | 1123 | 0 | 0 / 0 | 0.998473 |
| RED 128/256 KiB | 981.904 us | 1913.678 Gbps | 553.331 / 981.904 us | 568 | 67 | 65 / 87 | 0.992756 |

The no-ECN baseline has the highest goodput but exhibits 38 spurious transport
timeouts caused by long queuing delay; device queue tracing confirms that these
were not DropTail packet losses. The 64/96 KiB configuration reacts early and
is very fair, but its 1198 congestion-window reductions are too conservative.
The 128/256 KiB configuration reacts too late and reaches the queue limit.

For this workload, 96/160 KiB is the best tested lossless ECN point: it improves
goodput by about 6.1% over 64/96 KiB, retains 0.9985 Jain fairness, and completes
without drops, timeouts, or retransmissions. It is the recommended starting
point for subsequent scale and mixed-traffic experiments, not a universal
production setting.

## Reproduction

Run `scripts/uec/run_leaf_spine_ecn_sweep_800g.sh [output-directory]` from WSL2
after building the optimized ns-3 tree.
