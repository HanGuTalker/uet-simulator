# 128-node fat-tree validation at 800 Gbps

The standard large comparison topology is a k=8 three-tier fat-tree with 128 endpoints, 8 pods,
32 edge switches, 32 aggregation switches and 16 core switches. Each edge attaches four endpoints
and has four equal-cost aggregation uplinks; each aggregation switch has four core uplinks. The
topology contains 384 full-duplex point-to-point links, all configured at 800 Gbps with a 9000-byte
MTU.

The hop count and delay assumptions are unchanged from k=4, so the worst-case cross-pod base RTT
remains 5.1 us. Reproduce the topology readiness checks with:

```bash
bash scripts/transport/run_fat_tree_128_validation_800g.sh
```

The server validation produced:

| Workload | Completion | Goodput | Latency | Path coverage | Recovery |
|---|---:|---:|---:|---:|---:|
| Cross-pod 1 KiB single | 1/1 | 3.256 Gbps | 2.516 us | 2 aggregation / 2 core | none |
| 1 KiB directed All-to-All | 16,256/16,256 | 1018.603 Gbps aggregate | 3.065 us mean, 3.891 us P99 | 32 aggregation / 16 core | none |

The All-to-All readiness run uses a 1 us start gap to avoid turning the topology check into a
synchronized congestion experiment. It completed in 16.53 seconds of server wall-clock time with
approximately 493 MiB peak resident memory. The result verifies reachability, output scaling and
full ECMP-tier coverage; it is not a congestion-performance ranking.

Subsequent 128-node experiments should begin with a 127-to-1 ECN Incast at a small per-flow initial
window, then progress to All-to-All and collective workloads. Large payload sweeps should run as
independent processes with bounded parallelism because an individual ns-3 simulation remains
single-threaded. The first common congestion result is documented in
`docs/transport/fat-tree-128-incast-64k-800g.md`.
