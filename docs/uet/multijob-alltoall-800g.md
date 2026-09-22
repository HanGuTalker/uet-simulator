# Concurrent All-to-All validation at 800 Gbps

## Method

One, two, or four independent All-to-All groups start on the same eight-node,
4:1 oversubscribed leaf-spine fabric. Every group uses a distinct PDC per
source-destination pair and sends one 1 MiB message on every directed pair.
Message output includes `group_id` so completion time and fairness can be
calculated per job. RED uses the validated 96/160 KiB thresholds and a 512 KiB
limit.

## Results

| Configuration | Aggregate goodput | Makespan | RED drops | Timeouts / retransmits | Job Jain fairness |
|---|---:|---:|---:|---:|---:|
| 1 job, 64 KiB IW | 1518.732 Gbps | 309.312 us | 0 | 0 / 0 | 1.000000 |
| 2 jobs, 64 KiB IW | 1675.833 Gbps | 560.631 us | 335 | 296 / 391 | 0.999910 |
| 4 jobs, 64 KiB IW | 1758.327 Gbps | 1068.657 us | 908 | 845 / 1070 | 0.998436 |
| 4 jobs, 40 us job staggering | 1857.852 Gbps | 960.842 us | 198 | 194 / 336 | 0.998631 |
| 4 jobs, 16 KiB IW | 1818.925 Gbps | 1033.054 us | 0 | 0 / 0 | 0.999878 |
| 2 jobs, automatic 32 KiB IW | 1709.811 Gbps | 549.490 us | 0 | 0 / 0 | 0.999962 |
| 4 jobs, automatic 16 KiB IW | 1818.925 Gbps | 1033.054 us | 0 | 0 / 0 | 0.999878 |

With the default 64 KiB initial window, every independent PDC injects a full
window at the same time. The initial aggregate burst therefore scales with job
count faster than ECN feedback can return. The system remains fair and all
messages complete, but loss recovery becomes substantial.

Staggering reduces the synchronized burst but does not eliminate loss. Scaling
the per-PDC initial window from 64 KiB to 16 KiB for four active jobs preserves
the endpoint's aggregate 64 KiB startup budget. It eliminates queue drops,
timeouts, NACKs, and retransmissions while improving aggregate goodput by about
3.4% over the default four-job case.

The recommended control policy is therefore an endpoint-wide initial-window
budget divided across concurrently starting PDCs. A static 16 KiB value is only
the validated four-job point. The workload runner now provides
`--autoScaleInitialWindow=1`, which divides the requested initial window by the
number of concurrent All-to-All groups and enforces a one-MTU minimum. A
production implementation should obtain this concurrency count from admission
control rather than a command-line workload declaration.

## Reproduction

Run `scripts/uec/run_multijob_alltoall_800g.sh [output-directory]` after building
the optimized ns-3 tree.
