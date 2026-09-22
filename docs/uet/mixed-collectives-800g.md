# Mixed Ring AllReduce and All-to-All at 800 Gbps

## Workload

Two sequential 4 MiB-per-rank Ring AllReduce operations run concurrently with
one 1 MiB-per-pair All-to-All group on the eight-node, 4:1 oversubscribed
leaf-spine topology. The Ring has one PDC per endpoint while the All-to-All group
has seven PDCs per endpoint.

Automatic startup budgeting assigns 32 KiB to the Ring job and 32 KiB to the
background job at each endpoint. The background budget is divided across its
seven PDCs, producing a 4,681-byte initial window per background PDC.

## Results

| Ring placement | Ring collective times | Background completion | Marks | Drops / timeout / retransmit |
|---|---:|---:|---:|---:|
| Contiguous, equal startup budget per job | 620.783 / 279.169 us | 301.705 us | 50 | 0 / 0 / 0 |
| Interleaved, equal window per PDC | 853.914 / 742.503 us | 325.687 us | 292 | 0 / 0 / 0 |
| Interleaved, equal startup budget per job | 806.215 / 730.181 us | 347.720 us | 125 | 0 / 0 / 0 |

All mixed cases remain lossless. With contiguous rank placement, the first Ring
operation overlaps the background burst and slows; the second operation starts
after the background finishes and approaches the isolated Ring result. Group
budgeting also reduces this case to only 50 ECN marks.

Equal per-PDC windows favor All-to-All because it owns seven times as many PDCs
per endpoint. Dividing the background job budget across its PDCs improves the
interleaved Ring's first collective by about 5.6% and reduces ECN marks by 57%,
at the cost of a 6.8% increase in background completion time.

Startup-budget fairness does not provide complete steady-state isolation. The
group-budgeted interleaved Ring remains about 40% slower than its isolated first
collective, while the background All-to-All is about 12% slower than its
isolated run. Ring step barriers and per-PDC NSCC scheduling make a job-aware
rate scheduler or queue discipline the logical next experiment.

## Reproduction

Run `scripts/uec/run_mixed_collectives_800g.sh [output-directory]` after building
the optimized ns-3 tree.
