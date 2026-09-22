# 800 Gbps ECN closed-loop validation

The switched workload can install RED/ECN on the router egress toward the Incast receiver. The
point-to-point device queue is reduced to one packet so that backlog is held in the visible,
marking-capable queue disc rather than hidden in the device DropTail queue.

The initial scaled smoke test uses three senders, 1 MiB warm-up and two measured 1 MiB messages
per sender. RED starts marking at 64 KiB, reaches its maximum threshold at 96 KiB, and has a
512 KiB hard limit. NSCC target queue delay is 0.8 us.

Observed result:

- 6/6 measured messages completed
- 702.162 Gbps aggregate payload goodput
- 92,048-byte peak RED queue
- one RED CE mark, propagated as one endpoint ECN feedback event
- one NSCC multiplicative decrease from 172,032 to 86,016 bytes
- zero queue drops, retransmissions, timeouts, and NACKs

This verifies the complete queue buildup, CE marking, IP/UET feedback, and NSCC window-reduction
path. The smoke thresholds are deliberately scaled to the small validation workload; the formal
long-flow sweep must coordinate larger RED thresholds with the selected NSCC target delay.

Reproduce with `bash scripts/uec/run_ecn_incast_800g_smoke.sh`. Queue and congestion-window time
series are emitted as `closed-loop-incast-queue.csv` and `closed-loop-incast-cwnd.csv`.

