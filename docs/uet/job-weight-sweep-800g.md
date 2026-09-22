# Job-weight scheduling sweep at 800 Gbps

## Configuration

The workload combines an interleaved eight-rank Ring AllReduce with one
All-to-All background group on the 4:1 oversubscribed leaf-spine fabric. The
Ring sends two 4 MiB collectives; the background sends 1 MiB per directed pair.
Startup windows use equal per-job budgets. Static job weights set per-PDC NSCC
line-rate ceilings, and the Ring ceiling is released to 800 Gbps when all
background messages finish.

Isolated references are 573.814/615.888 us for the two Ring collectives and
309.312 us for the All-to-All group.

## Results

| Policy | Ring times | Ring slowdown | Background | Background slowdown | Marks | Loss/recovery |
|---|---:|---:|---:|---:|---:|---:|
| Startup budget only | 806.215 / 730.181 us | 40.5% / 18.6% | 347.720 us | 12.4% | 125 | none |
| Equal 1:1 | 809.423 / 680.233 us | 41.1% / 10.4% | 379.629 us | 22.7% | 171 | none |
| Ring 2:1 | 804.270 / 731.491 us | 40.2% / 18.8% | 355.000 us | 14.8% | 124 | none |
| Ring 4:1 | 728.135 / 733.938 us | 26.9% / 19.2% | 419.335 us | 35.6% | 65 | none |
| Ring 8:1 | 780.368 / 583.399 us | 36.0% / -5.3% | 690.567 us | 123.3% | 45 | none |

All policies remain lossless. No tested static weight meets both target SLOs of
at most 20% Ring slowdown and at most 50% background slowdown. Ring 4:1 is the
closest point, but its first collective is still 26.9% slower than isolated.

The response is non-monotonic. At 8:1 the heavily limited background remains
active for longer and overlaps almost the whole first Ring collective, making
that collective slower than at 4:1. Once the background completes, dynamic
bandwidth release allows the second Ring collective to recover.

The runtime ceiling release is functioning, but it does not immediately undo
congestion-window reductions accumulated earlier. A genuinely work-conserving
job scheduler needs to arbitrate queued packets across job groups and reassign
unused service every scheduling epoch; static per-PDC pacing ceilings are not a
substitute for such scheduling.

## Reproduction

Run `scripts/uec/run_job_weight_sweep_800g.sh [output-directory]` after building
the optimized ns-3 tree.
