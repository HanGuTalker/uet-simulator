# 4-node 800 Gbps collective experiments

## Configuration

- Four endpoints connected through one point-to-point switch/router.
- Full-duplex 800 Gbps endpoint links and 1 us propagation delay per hop.
- RED/ECN on every switch egress: 64 KiB minimum threshold, 96 KiB maximum
  threshold, and 512 KiB hard queue limit.
- UEC RUD transport with reused PDCs and a 1 MiB warm-up per communicating PDC.
- Two measured operations per case.

The Ring AllReduce workload models the network transfers of the standard ring
algorithm. Each operation has `N - 1` reduce-scatter steps followed by `N - 1`
all-gather steps. Every step transfers one `tensor_bytes / N` chunk from each
rank to its next neighbor and completes at a barrier before the next step. The
model does not add GPU reduction or kernel-launch time, so the reported time is
network collective time rather than end-to-end framework time.

## Results

| Pattern | Data size | Completion | Makespan | Aggregate wire goodput | ECN / drops / retransmits |
|---|---:|---:|---:|---:|---:|
| All-to-All | 1 MiB per peer message | 24/24 | 74.465 us | 2703.641 Gbps | 0 / 0 / 0 |
| All-to-All | 4 MiB per peer message | 24/24 | 272.738 us | 2952.674 Gbps | 0 / 0 / 0 |
| Ring AllReduce | 1 MiB tensor per rank | 48/48 transfers | 111.193 us | 905.302 Gbps | 0 / 0 / 0 |
| Ring AllReduce | 4 MiB tensor per rank | 48/48 transfers | 261.812 us | 1537.948 Gbps | 0 / 0 / 0 |

All-to-All uses 84.5% and 92.3% of the four endpoint receive links in the two
cases. Its largest observed egress queues were 30,184 and 58,232 bytes, both
below the 64 KiB ECN threshold.

Ring AllReduce collective latencies were 61.558/49.635 us for the two 1 MiB
operations and 159.784/102.028 us for the two 4 MiB operations. The second
operation is faster because its long-lived PDC congestion window is already
trained. Each ring egress carries one flow per step, and its peak queue was only
4,184 bytes; congestion feedback is therefore not expected in this balanced,
non-oversubscribed topology.

For Ring AllReduce, aggregate wire goodput counts all four ranks and all six
ring steps. The per-rank algorithm bandwidth over both operations is about
150.9 Gbps for 1 MiB and 256.4 Gbps for 4 MiB. Multiplying by the standard ring
bus-bandwidth factor `2 * (N - 1) / N = 1.5` gives about 226.3 and 384.5 Gbps
per rank, respectively.

## Reproduction

Run `scripts/uec/run_collectives_800g.sh [output-directory]` from WSL2 after
building the optimized ns-3 tree. Each case writes per-message, per-collective,
congestion-window, queue, throughput, CSV summary, and JSON summary files.
