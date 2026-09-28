# Falcon multi-seed fairness experiment at 800 Gbps

## Motivation and method

The 128-node, 4 MiB-per-sender Incast initially reported a Jain index of 0.33 for per-flow goodput.
To separate simulator insertion order from transport behavior, the workload runner now accepts a
reproducible `submissionSeed` and `startJitterNs`. A nonzero seed shuffles same-time source event
insertion order; jitter adds a seeded uniform start offset. Both values are recorded in schema-9
CSV and JSON output. `summarize_incast_fairness.py` derives Jain goodput, FCT variation and the
fastest source from per-message records.

Fifteen pre-fix runs used seeds 1--5 with 0, 100 and 1,000 ns jitter. Their aggregate goodput stayed
near 712--730 Gbps, but mean Jain indices were only 0.370, 0.404 and 0.397 respectively. The fastest
source was always endpoint 125--127 regardless of insertion order. These endpoints share the
receiver's edge switch, showing that the bias followed path length rather than event order.

## Falcon correction

The endpoint-wide 5.1 us worst-case base RTT had been applied to every Swift connection. Same-edge
connections therefore received a much larger queue-delay allowance than cross-pod connections.
The adapter now starts conservatively with the topology target, then permits each connection to
lower it from EACK-derived minimum fabric delay plus the 800 ns queue budget. A congested first
sample cannot raise the target. The initial delay sample retains Swift EWMA smoothing so short
flows do not overreact before a stable measurement exists.

## Corrected results

Six 4 MiB runs (natural order plus seeds 1--5) all completed 127/127 messages with no queue drop,
timeout, NACK or retransmission.

| Seed | Goodput | Mean FCT | P99 FCT | Jain goodput | FCT CV |
|---:|---:|---:|---:|---:|---:|
| 0 | 764.191 Gbps | 3434.647 us | 5564.667 us | 0.594 | 0.441 |
| 1 | 752.221 Gbps | 3360.798 us | 5641.357 us | 0.572 | 0.446 |
| 2 | 763.906 Gbps | 3400.231 us | 5530.857 us | 0.599 | 0.437 |
| 3 | 765.164 Gbps | 3382.983 us | 5552.886 us | 0.599 | 0.445 |
| 4 | 768.084 Gbps | 3390.834 us | 5540.650 us | 0.585 | 0.434 |
| 5 | 761.601 Gbps | 3392.866 us | 5552.433 us | 0.594 | 0.444 |

Mean goodput is 762.528 Gbps and mean Jain fairness is 0.591. The corrected 64 KiB point also
improves from 554.765 to 664.497 Gbps, with mean FCT falling from 85.435 to 78.725 us and no loss.
The remaining Jain gap against the other protocol models should be evaluated on a uniform-hop
sender subset: this all-endpoint Incast intentionally mixes same-edge, same-pod and cross-pod RTTs.
