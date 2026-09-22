# 800 Gbps steady-state 3-to-1 Incast result

## Configuration

- Topology: four endpoints connected through a full-duplex IPv4 point-to-point router
- Traffic: endpoints 1, 2, and 3 simultaneously send to endpoint 4
- Link rate: 800 Gbps per endpoint link
- One-way link delay: 1 us; NSCC base RTT: 4.2 us
- Payload MTU: 4096 bytes; queue: 10,000 packets
- NSCC maximum window: 630,000 bytes per PDC
- Warm-up: one unmeasured 64 MiB message per sender
- Measurement: four sequential 64 MiB messages per sender on reused RUD PDCs

Reproduce with:

```bash
bash scripts/uec/run_steady_incast_800g.sh
```

## Aggregate result

| Metric | Result |
| --- | ---: |
| Completed messages | 12 / 12 |
| Measured payload | 768 MiB |
| Measurement makespan | 8.264990 ms |
| Payload goodput | 779.486841 Gbps |
| Payload utilization | 97.435855% |
| Modeled receiver-link wire rate | 796.614237 Gbps |
| Modeled receiver-link utilization | 99.576780% |
| Mean / P50 / P95 / P99 FCT | 2066.167 / 2066.216 / 2066.426 / 2066.426 us |
| Measurement-start congestion window | 630,000 bytes |
| Jain sender fairness | 0.999999998 |
| Retransmissions / timeouts / NACKs | 0 / 0 / 0 |
| ECN / trimming / MTU / CRC events | 0 | 

## Per-sender result

| Sender | Goodput | Mean FCT | P95 FCT | Completed |
| --- | ---: | ---: | ---: | ---: |
| Endpoint 1 | 259.854036 Gbps | 2066.048 us | 2066.342 us | 4 / 4 |
| Endpoint 2 | 259.834228 Gbps | 2066.206 us | 2066.384 us | 4 / 4 |
| Endpoint 3 | 259.828947 Gbps | 2066.248 us | 2066.426 us | 4 / 4 |

The three senders divide the receiver bottleneck almost exactly equally. Aggregate payload
goodput remains close to the 800 Gbps single-flow baseline, while mean 64 MiB FCT is 2.994 times
the single-flow value. This is the expected result for three equal long-lived flows sharing one
receiver-facing link, with no loss or congestion marking.

Modeled wire utilization adds the same 90 bytes per 4096-byte data datagram used in the single
flow analysis. It represents the ns-3 point-to-point serializer rather than physical Ethernet
PCS/MAC overhead.

Structured aggregate, per-message, per-sender, and link-accounting outputs are under
`results/steady-incast-800g`.

