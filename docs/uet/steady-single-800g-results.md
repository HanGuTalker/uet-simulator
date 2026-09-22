# 800 Gbps steady-state single-flow result

## Configuration

The experiment uses the same workload and timing assumptions as the 400 Gbps steady-state
baseline, changing only the physical link rate and the BDP-derived NSCC maximum window.

- Topology: four endpoints connected through a full-duplex IPv4 point-to-point router
- Flow: endpoint 1 to endpoint 4
- Link rate: 800 Gbps per endpoint link
- One-way delay: 1 us per link; NSCC base RTT: 4.2 us
- Payload MTU: 4096 bytes; queue: 10,000 packets
- Maximum NSCC window: 630,000 bytes (1.5 times the 420,000-byte BDP)
- Warm-up: one unmeasured 64 MiB message
- Measurement: four sequential 64 MiB messages on the same RUD PDC

Reproduce with:

```bash
bash scripts/uec/run_steady_single_800g.sh
```

## Result

| Metric | 400 Gbps | 800 Gbps |
| --- | ---: | ---: |
| Completed messages | 4 / 4 | 4 / 4 |
| Payload goodput | 389.505 Gbps | 777.882 Gbps |
| Payload utilization | 97.376% | 97.235% |
| Modeled forward-wire rate | 398.064 Gbps | 794.974 Gbps |
| Modeled forward-link utilization | 99.516% | 99.372% |
| 64 MiB message FCT | 1378.340 us | 690.170 us |
| Measurement-start congestion window | 315,000 B | 630,000 B |
| Retransmissions / timeouts / NACKs | 0 / 0 / 0 | 0 / 0 / 0 |
| ECN / trimming / MTU / CRC events | 0 | 0 |

800 Gbps payload goodput is 1.9971 times the 400 Gbps result. Message FCT is 50.07% of the
400 Gbps result, a 49.93% reduction. This demonstrates near-linear bandwidth scaling when the
NSCC maximum window is scaled with BDP.

The modeled forward-wire calculation adds 90 bytes per measured data datagram: 12-byte PDS,
44-byte SES, 4-byte UET CRC, 28-byte IPv4/UDP, and 2-byte PPP. It describes the ns-3
point-to-point serializer, not physical 800GbE PCS/MAC overhead.

Structured outputs are under `results/steady-800g`, with the cross-rate comparison in
`results/steady-400g-800g-comparison.csv`.

