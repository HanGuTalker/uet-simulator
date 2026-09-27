# Unified 800 Gbps uncongested baseline

## Method

This baseline uses the common workload entry point for UEC, RoCEv2, veRoCE, MRC and Falcon. Each
run sends one message from endpoint 0 to endpoint 3 across the same two-leaf, four-spine fabric.
All links operate at 800 Gbps. The low-latency timing profile uses 100 ns endpoint-to-leaf
propagation, 250 ns leaf-to-spine propagation and 250 ns processing at every forwarding switch.
The resulting topology-derived base RTT is 3.1 us.

ECN and packet trimming are disabled and no background traffic is present. Every protocol starts
with the same 465,000-byte window: 1.5 times the 310,000-byte bandwidth-delay product. This is
large enough to fill the path, does not rely on protocol-specific window clamping and prevents the
experiment from becoming a fixed 64 KiB window benchmark.

The five payload sizes are 1 KiB, 4 KiB, 64 KiB, 1 MiB and 16 MiB. All 25 runs completed. Across
the complete matrix there were zero retransmissions, timeouts, NACKs, ECN marks, queue drops and
device drops.

Reproduce the experiment with:

```bash
bash scripts/transport/run_uncongested_baseline_800g.sh
```

The compact output is `results/uncongested-baseline-800g/baseline-summary.csv`; protocol-native
schema-version-7 outputs remain below the corresponding payload-size directories.

## Goodput

Application goodput in Gbps:

| Protocol | 1 KiB | 4 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---:|---:|---:|---:|---:|
| UEC | 5.483 | 20.252 | 233.224 | 680.452 | 773.108 |
| RoCEv2 | 5.483 | 20.302 | 233.640 | 680.673 | 773.126 |
| veRoCE | 5.483 | 20.252 | 233.224 | 680.341 | 772.952 |
| MRC | 5.483 | 20.252 | 233.224 | 680.452 | 773.108 |
| Falcon | 5.483 | 20.252 | 233.224 | 680.452 | 773.108 |

At 16 MiB, the protocols deliver 96.62--96.64% of the configured physical link rate as
application payload goodput.

## Message latency

Completion latency in microseconds (one observation per protocol and size, so mean and P99 are
equal):

| Protocol | 1 KiB | 4 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---:|---:|---:|---:|---:|
| UEC | 1.494 | 1.618 | 2.248 | 12.328 | 173.608 |
| RoCEv2 | 1.494 | 1.614 | 2.244 | 12.324 | 173.604 |
| veRoCE | 1.494 | 1.618 | 2.248 | 12.330 | 173.643 |
| MRC | 1.494 | 1.618 | 2.248 | 12.328 | 173.608 |
| Falcon | 1.494 | 1.618 | 2.248 | 12.328 | 173.608 |

## Fabric wire cost

The following ratio divides observed fabric bytes, including reverse control traffic, by completed
application payload bytes:

| Protocol | 1 KiB | 4 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---:|---:|---:|---:|---:|
| UEC | 1.168 | 1.042 | 1.038 | 1.038 | 1.038 |
| RoCEv2 | 1.094 | 1.023 | 1.023 | 1.023 | 1.023 |
| veRoCE | 1.117 | 1.029 | 1.030 | 1.031 | 1.031 |
| MRC | 1.197 | 1.049 | 1.049 | 1.049 | 1.049 |
| Falcon | 1.156 | 1.039 | 1.039 | 1.039 | 1.039 |

## Interpretation

The corrected baseline behaves as expected. Small-message latency is dominated by propagation,
switch processing and serialization; all five protocols therefore converge at approximately
1.5 us for 1 KiB. As payload size increases, link serialization dominates and all five transports
approach the 800 Gbps physical rate.

The earlier approximately 60 Gbps RoCEv2/Falcon plateau was not an intrinsic protocol result. It
was caused by a 64 KiB initial transport window combined with the old delay model. With an equal,
BDP-sufficient starting window, their unloaded throughput matches the other protocols.

RoCEv2 has the lowest modeled wire overhead, while MRC has the highest because of its additional
modeled control behavior. These differences do not materially affect unloaded large-message
completion time. Protocol performance separation should therefore be evaluated in the next
contention experiments using Incast, All-to-All and collective traffic with finite queues, ECN and
packet trimming where applicable.

These are deterministic single-message points. They establish the corrected unloaded operating
point and implementation consistency, but are not a congestion-control ranking.
