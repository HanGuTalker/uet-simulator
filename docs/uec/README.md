# UEC AI Base simulation

This directory defines the engineering baseline for the UEC AI Base simulation module.

- `specification-baseline.md` pins the normative source and amendment policy.
- `simulation-scope.md` records fidelity decisions and explicit exclusions.
- `implementation-inventory.md` records the frozen code inventory and unresolved multipath gap.
- `development-environment.md` records the reproducible build environment.
- `packet-model.md` defines implemented UEC wire codecs and their migration state.
- `wire-format-migration.md` tracks removal of the legacy behavioral envelope.
- `pdc-lifecycle.md` defines PDC state transitions and receive dispatch rules.
- `rud-model.md` defines the minimum reliable unordered delivery loop.
- `rod-model.md` defines reliable ordered delivery and Go-Back-N recovery.
- `uud-model.md` defines connectionless single-packet unreliable datagrams.
- `ai-base-coverage.md` maps every mandatory AI Base matrix row to implementation and test evidence.
- `conformance-audit.md` records the completed profile audit and fidelity boundaries.
- `trace-contract.md` defines stable trace names and payloads.
- `validation.md` records the repeatable test matrix and latest verified results.
- `performance-output.md` defines workload metrics and the CSV/JSON output schema.
- `baseline-freeze-800g.md` freezes the UEC reference used by later multi-protocol comparisons.
- `baseline-400g-results.md` records the completed unloaded 400 Gbps switched-fabric experiment.
- `../../requirements/ai-base.yaml` is the machine-readable requirement index.
- `../../requirements/uec-baseline-800g.json` is the machine-readable frozen baseline manifest.

The implementation is a packet-level behavioral simulator. It is not a UEC compliance
certification tool.

The protocol-neutral interface used for multi-transport comparisons is documented in
`../transport/common-architecture.md`.
