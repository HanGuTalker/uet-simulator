# 400 Gbps steady-state single-flow result

## Purpose

This experiment separates short-flow cold start from steady-state link capacity. It uses one
long-lived RUD PDC, warms NSCC with an unmeasured 64 MiB transfer, and then measures four
back-to-back 64 MiB messages. The workload runner submits the next measured message when the
previous message completes so that the PDC remains active without exceeding the signed 16-bit
`CLEAR_PSN` relative window.

## Configuration

- Topology: four endpoints connected through a full-duplex IPv4 point-to-point router
- Selected flow: endpoint 1 to endpoint 4 (`single`)
- Link rate: 400 Gbps per endpoint link
- One-way delay: 1 us per link
- Payload MTU: 4096 bytes
- Queue: 10,000 packets
- NSCC base RTT: 4.2 us
- NSCC target queue delay: 12 us
- Maximum NSCC window: 315,000 bytes
- Warm-up: one unmeasured 64 MiB message
- Measurement: four sequential 64 MiB messages on the same PDC

Reproduce with:

```bash
bash scripts/uec/run_steady_single_400g.sh
```

## Result

| Metric | Result |
| --- | ---: |
| Completed messages | 4 / 4 |
| Measured payload | 256 MiB |
| Measurement makespan | 5.513360 ms |
| Per-message latency | 1.378340 ms |
| Payload goodput | 389.505428 Gbps |
| Payload utilization | 97.376357% |
| Modeled forward-wire rate | 398.063897 Gbps |
| Modeled forward-link utilization | 99.515974% |
| Measurement-start congestion window | 315,000 bytes |
| Retransmissions / timeouts / NACKs | 0 / 0 / 0 |
| ECN marks / trimmed packets | 0 / 0 |
| MTU drops / CRC drops | 0 / 0 |

The payload utilization is `payload goodput / 400 Gbps`. The modeled forward-wire result adds
90 bytes per 4096-byte data datagram: 12-byte PDS Request, 44-byte SES Standard, 4-byte UET CRC,
28-byte IPv4/UDP, and 2-byte PPP. For 65,536 measured data datagrams this is:

```text
(268,435,456 + 65,536 * 90) * 8 / 0.005513360 = 398.063897 Gbps
```

This is the utilization of the ns-3 point-to-point serialization model. It is not a physical
400GbE PCS/MAC utilization figure: Ethernet preamble, inter-packet gap, RS-FEC, lane encoding,
and real NIC scheduling are not modeled by this topology.

## Interpretation

The earlier cold 16 MiB single-flow run reached 329.44 Gbps. Reusing a warmed PDC and calibrating
the unloaded RTT raises payload goodput to 389.51 Gbps and fills 99.52% of the modeled forward
link once protocol and network headers are included. Therefore the earlier low result was mainly
the expected initial-window/NSCC convergence cost, not a 400 Gbps topology cap.

Structured outputs are in `results/steady-400g/steady-single-{messages.csv,summary.csv,summary.json}`.

