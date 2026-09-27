# Performance statistics and structured output

`uet-ai-workload-example` emits nine files for every run. The base name is
`<outputPrefix>-<pattern>`:

- `-messages.csv`: one row per offered message, including source, target, payload size, scheduled
  and actual submission times, completion status, completion time and message latency;
- `-summary.csv`: one analysis-friendly row containing the run configuration and aggregate metrics;
- `-summary.json`: the same aggregate data grouped into messages, timing, latency, protocol events
  and transport counters;
- `-collectives.csv`: one row per modeled collective;
- `-cwnd.csv`: congestion-window changes over time;
- `-queue.csv`: queue occupancy changes over time;
- `-throughput.csv`: receiver payload goodput in fixed time bins;
- `-paths.csv`: every transport path selection with timestamp, source, connection, sequence and
  path identifier;
- `-fabric-paths.csv`: every packet transmitted on a monitored fabric uplink, with its timestamp,
  tier-local endpoints, on-wire size and whether that row participates in aggregate wire-byte
  accounting (empty for fabrics without monitored uplinks).

Times are integer nanoseconds. `goodput_bps` counts successfully delivered application payload bits
over the interval from first submission to final completion. Latency percentiles use the nearest-rank
definition over completed messages. An unsubmitted or incomplete message has `-1` for unavailable
timestamps/latency in the per-message CSV.

Protocol counters include retransmissions, timeouts, NACKs, received ECN marks, trimmed packets
and endpoint-triggered path reroutes.
Transport counters include transmitted/received UDP datagrams plus path-MTU and CRC drops. These
counters are aggregate totals across every endpoint in the run.

New outputs use schema version 8 and include the canonical `protocol`, physical fabric path counters,
spine count, split link delays, switch-processing delay and topology-derived base RTT in JSON and
CSV rows. Schema 8 adds fat-tree k/pod/edge/aggregation/core inventory and active aggregation/core
counts. Its fabric trace retains the schema-7 first four columns and appends
`from_tier,from_id,to_tier,to_id,wire_accounting`.
Schema version 1 files created before the common transport migration remain valid frozen UEC
artifacts; they implicitly describe UEC.

Example:

```bash
./ns3 run "uet-ai-workload-example --transport=uec --pattern=incast \
  --nodes=4 --messages=4 --payloadBytes=4096 \
  --outputPrefix=results/uec-4node"
```

This creates the nine files described above with the base name
`results/uec-4node-incast`.

For the reproducible 400 Gbps no-congestion baseline sweep, run
`bash scripts/uec/run_baseline_400g.sh`. It uses a four-endpoint, full-duplex point-to-point ToR
topology and tests single, incast and all-to-all traffic at 1 KiB, 4 KiB, 64 KiB, 1 MiB and
16 MiB. The combined result is `results/baseline-400g/baseline-400g-summary.csv`.
