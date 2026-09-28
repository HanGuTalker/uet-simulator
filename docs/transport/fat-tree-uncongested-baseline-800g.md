# 800 Gbps fat-tree no-congestion baseline

## Purpose and setup

This experiment isolates transport serialization and startup behavior before introducing shared-
link congestion. UEC, RoCEv2, veRoCE, MRC and Falcon send the same single cross-pod message over the
k=4 three-tier fat-tree. Every link is full-duplex 800 Gbps with a 9000-byte MTU. Host, fabric and
switch-processing delays are 100 ns, 250 ns and 250 ns respectively, producing a topology-derived
5.1 us worst-case RTT.

ECN and packet trimming are disabled, queues are uncongested, and every adapter receives the same
765,000-byte initial window (1.5 times the 510,000-byte BDP). The run is deterministic; its purpose
is not statistical inference. Reproduce and validate all 20 configurations with:

```bash
bash scripts/transport/run_fat_tree_uncongested_baseline_800g.sh
```

Each ns-3 run is single-threaded, but independent matrix points can run concurrently. On a server,
set a bounded worker count to use multiple cores while leaving capacity for other users:

```bash
JOBS=10 bash scripts/transport/run_fat_tree_uncongested_baseline_800g.sh
```

## Results

Every configuration completed its message without retransmission, timeout, NACK, ECN mark or queue
drop.

| Size | Protocol | Goodput (Gbps) | Line-rate efficiency | FCT (us) | Fabric bytes / payload byte | Active aggregation/core |
|---:|---|---:|---:|---:|---:|---:|
| 2 MiB | UEC | 694.306 | 86.788% | 24.164 | 1.040 | 2 / 2 |
| 2 MiB | RoCEv2 | 694.479 | 86.810% | 24.158 | 1.020 | 2 / 2 |
| 2 MiB | veRoCE | 694.191 | 86.774% | 24.168 | 1.030 | 4 / 4 |
| 2 MiB | MRC | 694.306 | 86.788% | 24.164 | 1.050 | 3 / 3 |
| 2 MiB | Falcon | 694.306 | 86.788% | 24.164 | 1.040 | 2 / 2 |
| 16 MiB | UEC | 768.311 | 96.039% | 174.692 | 1.040 | 2 / 2 |
| 16 MiB | RoCEv2 | 768.337 | 96.042% | 174.686 | 1.020 | 2 / 2 |
| 16 MiB | veRoCE | 768.157 | 96.020% | 174.727 | 1.030 | 4 / 4 |
| 16 MiB | MRC | 768.311 | 96.039% | 174.692 | 1.050 | 3 / 3 |
| 16 MiB | Falcon | 768.311 | 96.039% | 174.692 | 1.040 | 2 / 2 |
| 64 MiB | UEC | 777.186 | 97.148% | 690.788 | 1.040 | 2 / 2 |
| 64 MiB | RoCEv2 | 777.193 | 97.149% | 690.782 | 1.020 | 2 / 2 |
| 64 MiB | veRoCE | 777.031 | 97.129% | 690.926 | 1.030 | 4 / 4 |
| 64 MiB | MRC | 777.186 | 97.148% | 690.788 | 1.050 | 3 / 3 |
| 64 MiB | Falcon | 777.186 | 97.148% | 690.788 | 1.040 | 2 / 2 |
| 128 MiB | UEC | 778.685 | 97.336% | 1378.916 | 1.040 | 2 / 2 |
| 128 MiB | RoCEv2 | 778.689 | 97.336% | 1378.910 | 1.020 | 2 / 2 |
| 128 MiB | veRoCE | 778.530 | 97.316% | 1379.191 | 1.030 | 4 / 4 |
| 128 MiB | MRC | 778.685 | 97.336% | 1378.916 | 1.050 | 3 / 3 |
| 128 MiB | Falcon | 778.685 | 97.336% | 1378.916 | 1.040 | 2 / 2 |

For 128 MiB, the interior 10 us receiver-throughput bins average 780.03--780.19 Gbps and peak at
786.43 Gbps. The aggregate FCT spread between the fastest and slowest protocol is 0.281 us, less
than 0.03%.

## Interpretation

The progression from about 694 Gbps at 2 MiB to about 778.6 Gbps at 128 MiB shows the fixed
cross-pod startup/feedback delay being amortized. The remaining gap from 800 Gbps is predominantly
the common packetization and wire-header cost; it is not evidence of congestion because all queue
and recovery counters remain zero.

UEC, RoCEv2 and Falcon remain flow-pinned in this run. veRoCE exercises all four physical core
paths through source-port entropy changes; MRC reaches three. Packet spreading provides no
throughput advantage for a single uncongested flow because one 800 Gbps host link is already the
bottleneck. Its value must be evaluated under contention and asymmetric path load.

This baseline therefore establishes that the common topology and workload do not intrinsically
penalize RoCEv2 or Falcon. The next comparison should introduce synchronized cross-pod Incast with
finite queues and ECN so congestion-control convergence, queue occupancy, fairness and tail FCT can
differentiate the transports.
