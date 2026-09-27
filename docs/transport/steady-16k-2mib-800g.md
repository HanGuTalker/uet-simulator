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

## 64 MiB scaling check

Setting `TOTAL_BYTES=67108864` sends 4,096 consecutive 16 KiB messages per protocol:

| Protocol | Completed | Goodput | Physical-rate fraction | Mean latency | P99 latency |
|---|---:|---:|---:|---:|---:|
| UEC | 4096/4096 | 778.408 Gbps | 97.30% | 9.934 us | 17.964 us |
| RoCEv2 | 4096/4096 | 778.412 Gbps | 97.30% | 9.930 us | 17.960 us |
| veRoCE | 4096/4096 | 778.252 Gbps | 97.28% | 10.003 us | 18.100 us |
| MRC | 4096/4096 | 778.408 Gbps | 97.30% | 9.934 us | 17.964 us |
| Falcon | 4096/4096 | 778.408 Gbps | 97.30% | 9.934 us | 17.964 us |

All 20,480 messages completed without retransmission, timeout, NACK, ECN marking or packet loss.
The longer transfer amortizes almost all fixed pipeline latency and approaches the approximately
782.8 Gbps UEC payload-rate ceiling imposed by 4 KiB segmentation and wire headers. The submission
rate represents 800 Gbps of payload before headers, so it slightly exceeds the sustainable payload
rate; this creates bounded serialization backlog and explains the higher per-message mean and P99
latency without indicating congestion loss.

## 128 MiB scaling check

At `TOTAL_BYTES=134217728`, each protocol sends 8,192 consecutive messages:

| Protocol | Completed | Goodput | Physical-rate fraction | Mean latency | P99 latency |
|---|---:|---:|---:|---:|---:|
| UEC | 8192/8192 | 779.298 Gbps | 97.41% | 18.126 us | 34.184 us |
| RoCEv2 | 8192/8192 | 779.300 Gbps | 97.41% | 18.122 us | 34.180 us |
| veRoCE | 8192/8192 | 779.143 Gbps | 97.39% | 18.263 us | 34.456 us |
| MRC | 8192/8192 | 779.298 Gbps | 97.41% | 18.126 us | 34.184 us |
| Falcon | 8192/8192 | 779.298 Gbps | 97.41% | 18.126 us | 34.184 us |

The UEC result improves by only 0.890 Gbps over the 64 MiB run. Four serialized data packets make
each 16 KiB message occupy approximately 168 ns of service time, so the workload's practical
payload-rate asymptote is about 780.19 Gbps. The 128 MiB result is already within 0.9 Gbps of this
limit. Submission remains slightly faster than service, so queueing latency grows with run length;
all runs nevertheless complete without retransmission, timeout, marking or loss.
