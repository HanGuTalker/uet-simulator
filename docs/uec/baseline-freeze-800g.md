# Frozen UEC AI Base 800 Gbps baseline

## Purpose

This snapshot freezes the UEC AI Base reference point that subsequent veRoCE, MRC, Falcon and
MetaRoCE implementations will use for comparison. It records previously generated results and does
not claim that a new regression run was performed during the freeze.

- Baseline ID: `uec-ai-base-800g-2026-09-20`
- Specification: UEC 1.0.3, AI Base profile
- Simulator: ns-3.47
- Frozen source revision: `9ba92c6eada85519f42abedea86137b5cd686859`
- Snapshot date: 2026-09-23
- Long-running regression executed on snapshot date: **no**
- Machine-readable manifest: `requirements/uec-baseline-800g.json`

The 20 mandatory AI Base rows remain recorded as verified in `requirements/ai-base.yaml`. The
historical verification evidence is dated 2026-09-20 and is documented in `validation.md`.

## Frozen headline results

All links use 800 Gbps per port. Goodput is application payload goodput unless explicitly described
as modeled wire goodput.

| Workload | Topology | Completion | Goodput | Mean / P99 latency | ECN marks | Retransmissions | Fairness or utilization |
|---|---|---:|---:|---:|---:|---:|---:|
| Single, 64 MiB | 4-node switched | 4/4 | 777.882 Gbps | 690.170 / 690.170 us | 0 | 0 | 97.235% payload utilization |
| 3-to-1 Incast, 64 MiB | 4-node switched | 12/12 | 779.487 Gbps | 2066.167 / 2066.426 us | 0 | 0 | Jain 0.999999998 |
| All-to-All, 4 MiB | 4-node switched | 24/24 | 2952.674 Gbps | 136.055 / 138.496 us | 0 | 0 | 92.3% aggregate receive-link utilization |
| Ring AllReduce, 4 MiB/rank | 4-node switched | 48/48 transfers | 1537.948 Gbps modeled wire | 21.818 / 37.512 us per transfer | 0 | 0 | 159.784 / 102.028 us collective time |
| All-to-All, 16 MiB | 8-node 4:1 leaf-spine | 56/56 | 1982.222 Gbps | 2388.292 / 3791.802 us | 4027 queue marks | 0 | Jain 0.999767 |
| Interleaved Ring AllReduce, 4 MiB/rank | 8-node leaf-spine | 224/224 transfers | 789.714 Gbps modeled wire | 33.906 / 58.256 us per transfer | 38 queue marks | 0 | no queue drops |

The first four cases establish unloaded or balanced throughput behavior. The final two cases are the
frozen congestion-control references: they trigger ECN/NSCC while completing without queue loss or
transport retransmission. Metrics with different accounting domains—payload goodput, modeled wire
goodput and collective completion time—must not be compared as if they were identical quantities.

## Lightweight snapshot check

The result directory is intentionally ignored by Git because traces can be large. On a machine that
contains the original result artifacts, validate the snapshot without building ns-3 or launching a
simulation:

```bash
python3 scripts/uec/check_baseline_snapshot.py
```

The checker confirms the mandatory requirement statuses, referenced scripts and documentation, and
the recorded values in six existing JSON summaries. A different Git revision produces a warning by
default because later protocol work will naturally advance the repository. Use
`--strict-revision` only when checking the original frozen checkout.

## Use in future protocol comparisons

Every new transport must use the same link rate, topology, propagation delay, workload size, start
schedule and metric definitions before its result is placed beside this baseline. Protocol-specific
window, pacing, credit and congestion-control parameters must be reported separately rather than
silently copied from UEC NSCC.

This snapshot is engineering evidence for the ns-3 behavioral model, not UEC certification or NIC
interoperability evidence.
