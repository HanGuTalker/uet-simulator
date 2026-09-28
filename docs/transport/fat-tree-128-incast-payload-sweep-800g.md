# 128-node Incast payload sweep at 800 Gbps

## Configuration

This sweep extends the standard k=8 fat-tree 127-to-1 Incast experiment from 64 KiB to 256 KiB,
1 MiB and 4 MiB per sender. All points retain the same 800 Gbps links, 4,608-byte initial window,
64/96 KiB RED thresholds, 512 KiB hard queue limit and 800 ns target queue delay. A larger payload
therefore changes only the duration of synchronized congestion, not the topology or control
parameters.

## Aggregate results

| Payload per sender | Protocol | Goodput | Mean FCT | P99 FCT | Queue drops | Retransmissions |
|---:|---|---:|---:|---:|---:|---:|
| 64 KiB | UEC | 703.750 Gbps | 81.370 us | 94.530 us | 0 | 0 |
| 64 KiB | RoCEv2 | 753.663 Gbps | 81.538 us | 88.307 us | 0 | 0 |
| 64 KiB | veRoCE | 730.246 Gbps | 84.532 us | 91.108 us | 0 | 0 |
| 64 KiB | MRC | 709.418 Gbps | 80.400 us | 93.732 us | 0 | 0 |
| 64 KiB | Falcon | 554.765 Gbps | 85.435 us | 112.579 us | 0 | 0 |
| 256 KiB | UEC | 266.295 Gbps | 348.547 us | 970.328 us | 13 | 26 |
| 256 KiB | RoCEv2 | 765.048 Gbps | 328.392 us | 348.092 us | 0 | 0 |
| 256 KiB | veRoCE | 740.090 Gbps | 340.549 us | 359.754 us | 0 | 0 |
| 256 KiB | MRC | 726.303 Gbps | 327.127 us | 365.662 us | 0 | 0 |
| 256 KiB | Falcon | 538.027 Gbps | 308.003 us | 494.088 us | 0 | 0 |
| 1 MiB | UEC | 718.352 Gbps | 1288.155 us | 1455.789 us | 17 | 34 |
| 1 MiB | RoCEv2 | 767.891 Gbps | 1315.784 us | 1387.334 us | 0 | 0 |
| 1 MiB | veRoCE | 742.923 Gbps | 1364.490 us | 1433.937 us | 0 | 0 |
| 1 MiB | MRC | 757.527 Gbps | 1309.589 us | 1405.978 us | 0 | 0 |
| 1 MiB | Falcon | 624.205 Gbps | 1137.545 us | 1668.235 us | 2 | 2 |
| 4 MiB | UEC | 752.663 Gbps | 5205.445 us | 5639.739 us | 48 | 96 |
| 4 MiB | RoCEv2 | 768.631 Gbps | 5265.443 us | 5544.118 us | 0 | 0 |
| 4 MiB | veRoCE | 743.564 Gbps | 5460.274 us | 5731.019 us | 0 | 0 |
| 4 MiB | MRC | 762.413 Gbps | 5246.045 us | 5589.080 us | 0 | 0 |
| 4 MiB | Falcon | 725.959 Gbps | 3638.324 us | 5829.542 us | 1 | 1 |

All 127 messages completed at every point. Runs with queue drops are valid completed experiments but
do not pass the suite's stricter lossless validator.

## Interpretation

Falcon rises from 554.765 Gbps at 64 KiB to 725.959 Gbps at 4 MiB. This confirms that the corrected
live Swift window supports high steady-state utilization and that a material part of the short-flow
gap is window startup. RoCEv2, veRoCE and MRC are already near their steady aggregate rates at
256 KiB. UEC reaches 752.663 Gbps at 4 MiB, but its synchronized control oscillation hits the hard
queue limit at every extended point and causes timeout plus NACK recovery.

The 4 MiB per-flow completion distribution exposes a separate fairness issue:

| Protocol | Jain index of per-flow goodput | FCT coefficient of variation | Minimum / maximum FCT |
|---|---:|---:|---:|
| UEC | 0.85 | 0.16 | 1387.40 / 5661.78 us |
| RoCEv2 | 0.89 | 0.15 | 1666.68 / 5544.16 us |
| veRoCE | 0.92 | 0.14 | 1898.17 / 5731.06 us |
| MRC | 0.85 | 0.17 | 1409.80 / 5589.37 us |
| Falcon | 0.33 | 0.43 | 237.40 / 5870.05 us |

Falcon's low mean FCT is therefore not uniform acceleration: some flows finish extremely early
while tail flows finish slightly later than the other protocols. The next experiment should vary
sender ordering and start-time jitter across multiple seeds before treating this as a protocol
property; simultaneous ns-3 events can otherwise create deterministic source-order capture.
