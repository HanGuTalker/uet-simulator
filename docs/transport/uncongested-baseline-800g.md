# Unified 800 Gbps uncongested baseline

> **Superseded timing profile.** The measurements below are retained as a historical artifact of
> the original 1 us-per-link model and 64 KiB initial-window experiment. They must not be used as
> the corrected low-latency baseline. The runner now uses 100 ns host links, 250 ns fabric links,
> 250 ns switch processing and a topology-derived base RTT; its replacement results have not yet
> been recorded in this document.

## Method

This baseline uses the common workload entry point for UEC, RoCEv2, veRoCE, MRC and Falcon. Each
run sends one message from endpoint 0 to endpoint 3 across the same two-leaf, four-spine fabric.
All links operate at 800 Gbps with 1 us one-way delay per hop. ECN and packet trimming are disabled,
the initial transport window is 65,536 bytes, and no background traffic is present.

The five payload sizes are 1 KiB, 4 KiB, 64 KiB, 1 MiB and 16 MiB. Every one of the 25 runs
completed successfully. All runs recorded zero retransmissions, timeouts, NACKs, ECN marks, queue
marks, queue drops and device drops. This confirms that the measurements are an unloaded transport
baseline rather than a congestion-control stress test.

Run the corrected replacement experiment with:

```bash
bash scripts/transport/run_uncongested_baseline_800g.sh
```

The compact replacement output is `results/uncongested-baseline-800g/baseline-summary.csv`;
protocol-native
structured outputs remain below the corresponding payload-size directories.

## Goodput

Application goodput in Gbps:

| Protocol | 1 KiB | 4 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---:|---:|---:|---:|---:|
| UEC | 2.026 | 7.862 | 35.920 | 74.680 | 203.368 |
| RoCEv2 | 2.026 | 7.869 | 42.528 | 58.681 | 60.108 |
| veRoCE | 2.026 | 7.862 | 42.487 | 58.624 | 60.049 |
| MRC | 2.026 | 7.862 | 42.487 | 81.386 | 206.566 |
| Falcon | 2.026 | 7.862 | 42.487 | 58.624 | 60.049 |

## Message latency

Completion latency in microseconds (one observation per protocol/size, so mean and P99 are equal):

| Protocol | 1 KiB | 4 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---:|---:|---:|---:|---:|
| UEC | 4.044 | 4.168 | 14.596 | 112.328 | 659.976 |
| RoCEv2 | 4.044 | 4.164 | 12.328 | 142.952 | 2232.936 |
| veRoCE | 4.044 | 4.168 | 12.340 | 143.092 | 2235.124 |
| MRC | 4.044 | 4.168 | 12.340 | 103.072 | 649.756 |
| Falcon | 4.044 | 4.168 | 12.340 | 143.092 | 2235.124 |

## Fabric wire cost

The following ratio divides all observed leaf-to-spine bytes, including reverse control traffic,
by completed application payload bytes:

| Protocol | 1 KiB | 4 KiB | 64 KiB | 1 MiB | 16 MiB |
|---|---:|---:|---:|---:|---:|
| UEC | 1.168 | 1.042 | 1.038 | 1.038 | 1.038 |
| RoCEv2 | 1.094 | 1.023 | 1.023 | 1.023 | 1.023 |
| veRoCE | 1.375 | 1.094 | 1.038 | 1.034 | 1.034 |
| MRC | 1.197 | 1.049 | 1.049 | 1.049 | 1.049 |
| Falcon | 1.156 | 1.039 | 1.039 | 1.039 | 1.039 |

## Interpretation

Small messages are dominated by the approximately 4 us propagation/control round trip, so all
models converge. At 64 KiB, RoCEv2, veRoCE, MRC and Falcon have nearly identical completion time;
UEC's modeled transaction/control path adds about 2.27 us.

For 1 MiB and 16 MiB, UEC and MRC grow or recycle sending credit during the transfer and reach
roughly 203--207 Gbps at 16 MiB. The current RoCEv2, veRoCE and Falcon comparison adapters remain
near the configured 64 KiB window operating point and plateau around 60 Gbps. This is transport
window behavior, not physical-link congestion: the 800 Gbps link is intentionally underfilled and
all queue/congestion counters remain zero.

RoCEv2 has the lowest modeled wire cost, while MRC pays about 4.9% at large sizes for its data and
control behavior. Packet spraying does not improve latency in this symmetric unloaded topology;
its value should appear in the later contention, imbalance and failure experiments.

These are deterministic single-message baseline points. They establish correctness and operating
points, but should not be presented as a statistically repeated congestion-performance ranking.
