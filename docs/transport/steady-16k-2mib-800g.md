# Steady 16 KiB messages over 2 MiB at 800 Gbps

## Method

Each transport sends 128 consecutive 16 KiB messages, for exactly 2 MiB of measured application
payload, over one reused connection. Message submissions are spaced by 164 ns, the rounded ideal
payload serialization interval at 800 Gbps. The common two-leaf, four-spine topology uses 100 ns
host links, 250 ns fabric links, 250 ns switch processing and a topology-derived 3.1 us base RTT.
The initial transport window is 465,000 bytes. ECN and trimming are disabled.

Run the experiment with:

```bash
bash scripts/transport/run_steady_16k_800g.sh
```

Set `TOTAL_BYTES` to another nonzero multiple of 16,384 to change the measured volume.

## Results

| Protocol | Completed | Goodput | Mean latency | P99 latency | Wire bytes / payload byte |
|---|---:|---:|---:|---:|---:|
| UEC | 128/128 | 726.916 Gbps | 1.998 us | 2.248 us | 1.039 |
| RoCEv2 | 128/128 | 727.042 Gbps | 1.994 us | 2.244 us | 1.023 |
| veRoCE | 128/128 | 726.790 Gbps | 2.000 us | 2.252 us | 1.030 |
| MRC | 128/128 | 726.916 Gbps | 1.998 us | 2.248 us | 1.049 |
| Falcon | 128/128 | 726.916 Gbps | 1.998 us | 2.248 us | 1.039 |

All runs recorded zero retransmissions, timeouts, NACKs, ECN marks, queue drops and device drops.

## Interpretation

The single-message 16 KiB result was approximately 75 Gbps because its end-to-end Goodput
included the complete fixed path latency. Keeping one connection continuously supplied amortizes
that fixed cost and raises measured Goodput to approximately 727 Gbps, or 90.9% of the configured
physical rate, with only 2 MiB of total traffic. The remaining gap includes initial/final pipeline
latency and wire-format overhead. A substantially longer transfer should move the aggregate result
closer to the large-message asymptote, but is not necessary to demonstrate that the 75 Gbps result
was a short-message measurement effect rather than a link-rate limitation.
