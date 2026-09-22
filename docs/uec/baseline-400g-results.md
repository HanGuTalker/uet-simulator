# 400 Gbps switched-fabric baseline

Date: 2026-09-20. Build: ns-3.47 optimized. Profile: UEC 1.0.3 AI Base, RUD with NSCC.

## Configuration

- four endpoints connected through one IPv4-routing ToR node;
- dedicated full-duplex 400 Gbps point-to-point link per endpoint;
- 1 us one-way propagation delay per link, 9000-byte device MTU;
- 10,000-packet DropTail queues, with ECN, trimming and injected loss disabled;
- one message per communicating pair;
- single flow, three-to-one incast, and four-node all-to-all;
- application payload sweep: 1 KiB, 4 KiB, 64 KiB, 1 MiB and 16 MiB.

The single/incast efficiency denominator is the 400 Gbps receiver link. All-to-all efficiency uses
the four endpoints' 1.6 Tbps aggregate transmit ceiling. Goodput contains completed application
payload only. Small-workload makespan includes the deliberate 10 us endpoint start staggering.

## Results

| Pattern | Payload | Goodput (Gbps) | Ceiling efficiency | Mean latency (us) | P99 latency (us) |
|---|---:|---:|---:|---:|---:|
| Single | 1 KiB | 4.01 | 1.00% | 2.04 | 2.04 |
| Incast | 1 KiB | 1.11 | 0.28% | 2.04 | 2.04 |
| All-to-all | 1 KiB | 3.06 | 0.19% | 2.07 | 2.09 |
| Single | 4 KiB | 15.11 | 3.78% | 2.17 | 2.17 |
| Incast | 4 KiB | 4.43 | 1.11% | 2.17 | 2.17 |
| All-to-all | 4 KiB | 12.16 | 0.76% | 2.25 | 2.34 |
| Single | 64 KiB | 57.59 | 14.40% | 9.10 | 9.10 |
| Incast | 64 KiB | 54.04 | 13.51% | 9.10 | 9.10 |
| All-to-all | 64 KiB | 160.20 | 10.01% | 9.19 | 9.27 |
| Single | 1 MiB | 142.25 | 35.56% | 58.97 | 58.97 |
| Incast | 1 MiB | 293.56 | 73.39% | 65.55 | 65.73 |
| All-to-all | 1 MiB | 943.88 | 58.99% | 72.58 | 76.65 |
| Single | 16 MiB | 329.44 | 82.36% | 407.41 | 407.41 |
| Incast | 16 MiB | 384.36 | 96.09% | 1017.97 | 1027.60 |
| All-to-all | 16 MiB | 1413.55 | 88.35% | 1062.56 | 1109.41 |

All 15 runs completed 100% of submitted messages with zero retransmissions, timeouts, NACKs, MTU
drops and CRC drops. The acceptance criteria for an unloaded baseline are therefore satisfied.

## Interpretation

Short messages are dominated by the roughly 2 us protocol/network latency and, for aggregate
workloads, intentional start staggering. Efficiency rises with message size. At 16 MiB, incast
reaches 96.09% of the receiver-link ceiling; aggregate all-to-all reaches 88.35% of its four-link
ceiling; the single flow reaches 82.36%. The absence of recovery events confirms that these values
are no longer contaminated by the collisions present in the earlier shared-CSMA setup.

The next experiment can introduce a finite bottleneck queue and ECN thresholds, keeping this table
as the unloaded reference for NSCC comparisons.
