# 128-node 64 KiB ECN Incast at 800 Gbps

## Configuration

This experiment is the first congested comparison on the standard k=8 fat-tree. Endpoints 0--126
simultaneously send one 64 KiB message to endpoint 127, creating a 127-to-1 cross-pod Incast. UEC,
RoCEv2, veRoCE, MRC and Falcon use the same topology and workload:

- 128 endpoints, 32 edge, 32 aggregation and 16 core switches;
- 800 Gbps links, 100 ns host links, 250 ns fabric links and 250 ns switch processing;
- RED/ECN marking at 64/96 KiB and a 512 KiB queue limit;
- 800 ns target queue delay;
- one 4,608-byte initial window per sender; and
- no packet trimming.

The 4,608-byte window is deliberate. A 4,096-byte window cannot admit one 4,096-byte payload plus
the largest modeled protocol header in RoCEv2, veRoCE and Falcon. An 8,192-byte window produces a
first-RTT UEC burst larger than the 512 KiB queue and tests loss recovery instead of clean ECN
response. At 4,608 bytes every protocol can transmit one packet, UEC still triggers feedback, and
all five runs remain lossless.

Run the five protocol points concurrently on a server with:

```bash
JOBS=5 bash scripts/transport/run_fat_tree_128_incast_800g.sh
```

## Results

All 635 messages completed without queue drop, device drop, retransmission, timeout or NACK. The
complete matrix took 6.85 seconds of wall-clock time with five server workers.

| Protocol | Aggregate goodput | Mean FCT | P95 FCT | P99 FCT | Maximum FCT | Peak queue | Queue marks / received feedback | Aggregation / core coverage |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| UEC | 703.750 Gbps | 81.370 us | 92.724 us | 94.530 us | 94.614 us | 493,712 B | 711 / 711 | 32 / 16 |
| RoCEv2 | 753.663 Gbps | 81.538 us | 88.102 us | 88.307 us | 88.348 us | 339,480 B | 70 / 70 | 30 / 16 |
| veRoCE | 730.246 Gbps | 84.532 us | 90.898 us | 91.108 us | 91.181 us | 307,852 B | 99 / 82 | 32 / 16 |
| MRC | 709.418 Gbps | 80.400 us | 91.632 us | 93.732 us | 93.858 us | 416,400 B | 574 / 574 | 32 / 16 |
| Falcon | 554.765 Gbps | 85.435 us | 111.529 us | 112.579 us | 120.023 us | 348,768 B | 653 / 0 | 31 / 16 |

## Interpretation and limits

RoCEv2 has the highest aggregate goodput and lowest P99 in this short, lossless ECN burst. UEC and
MRC react to substantially more marks and trade some aggregate completion time for explicit queue
response; MRC has the lowest mean FCT, while UEC has the highest queue peak but remains below the
hard limit. veRoCE spreads across every aggregation and core switch and sits between RoCEv2 and
UEC/MRC in aggregate performance.

Falcon is intentionally not driven by IP ECN in the current comparison subset. Its Swift/RUE model
uses EACK-derived RTT and fabric-delay measurements, so the switch marked 653 packets while the
common `EcnReceived` counter remained zero. Falcon now uses its live Swift window rather than
mistakenly retaining the 4,608-byte initial seed as a permanent cap, and compares its round-trip
measurement with unloaded RTT plus the 800 ns queue-delay budget. Relative to the superseded run,
goodput rises from 233.164 to 554.765 Gbps and mean FCT falls from 258.872 to 85.435 us. The
remaining aggregate-goodput gap comes primarily from Swift's delay-based response and linear
window growth on this very short, synchronized 16-packet-per-sender workload. This is a result for
the documented Falcon subset and parameter defaults, not a claim about production Falcon
hardware.

Falcon now also emits its normalized live congestion-window trace. Queue, completion, path and
protocol-event metrics remain available for every adapter. The next experiment should sweep larger
per-sender payloads and at least two initial-window/queue configurations before drawing a general
congestion-control ranking.
