# Performance statistics and structured output

`uet-ai-workload-example` emits three files for every run. The base name is
`<outputPrefix>-<pattern>`:

- `-messages.csv`: one row per offered message, including source, target, payload size, scheduled
  and actual submission times, completion status, completion time and message latency;
- `-summary.csv`: one analysis-friendly row containing the run configuration and aggregate metrics;
- `-summary.json`: the same aggregate data grouped into messages, timing, latency, protocol events
  and transport counters.

Times are integer nanoseconds. `goodput_bps` counts successfully delivered application payload bits
over the interval from first submission to final completion. Latency percentiles use the nearest-rank
definition over completed messages. An unsubmitted or incomplete message has `-1` for unavailable
timestamps/latency in the per-message CSV.

Protocol counters include retransmissions, timeouts, NACKs, received ECN marks and trimmed packets.
Transport counters include transmitted/received UDP datagrams plus path-MTU and CRC drops. These
counters are aggregate totals across every endpoint in the run.

Example:

```bash
./ns3 run "uet-ai-workload-example --pattern=incast \
  --nodes=4 --messages=4 --payloadBytes=4096 \
  --outputPrefix=results/uec-4node"
```

This creates `results/uec-4node-incast-messages.csv`,
`results/uec-4node-incast-summary.csv`, and `results/uec-4node-incast-summary.json`.

For the reproducible 400 Gbps no-congestion baseline sweep, run
`bash scripts/uec/run_baseline_400g.sh`. It uses a four-endpoint, full-duplex point-to-point ToR
topology and tests single, incast and all-to-all traffic at 1 KiB, 4 KiB, 64 KiB, 1 MiB and
16 MiB. The combined result is `results/baseline-400g/baseline-400g-summary.csv`.
