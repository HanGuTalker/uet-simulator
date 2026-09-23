# Validation

## Automated coverage

The `uet` suite contains 25 focused cases covering byte-exact PDS/SES layouts, SES execution, PDC
lifecycle, RUD/ROD/UUD, CACK/SACK, PSN wrap, response retention/clear, NSCC, trimming, CRC32C, and
real IPv4/UDP plus IPv6/UDP point-to-point delivery. Reliability stress combines deterministic
Request loss, ACK loss and reordering; multi-PDC tests check state isolation.

The CI workflow runs a normal debug build and a second build with AddressSanitizer,
UndefinedBehaviorSanitizer and LeakSanitizer. Both use the same UET suite. Examples are separately
run as executable acceptance checks.

## Local acceptance commands

```bash
bash scripts/uec/configure.sh
bash scripts/uec/build.sh
bash scripts/uec/test.sh
./ns3 run uet-smoke-example
./ns3 run uet-rud-example
./ns3 run "uet-ai-workload-example --pattern=incast"
./ns3 run "uet-ai-workload-example --pattern=all-to-all"
```

Both workload commands accept `--outputPrefix`; their performance CSV/JSON schema is documented in
`performance-output.md`.

The sanitizer build uses `--enable-sanitizers` and directly executes the generated test runner with
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1`.

## Latest verified result

On 2026-09-20 in WSL2 with ns-3.47:

- UET suite: 25/25 cases passed in the normal debug build;
- all configured QUICK suites/examples: 153/153 passed;
- UET suite: 25/25 passed with address, undefined-behavior and leak sanitizers;
- smoke and RUD recovery examples passed;
- four-node incast completed 12/12 messages; and
- four-node all-to-all completed 48/48 messages.

The optimized 400 Gbps switched-fabric baseline completed all 15 pattern/message-size combinations
with 100% delivery and zero retransmissions, timeouts, NACKs, MTU drops, or CRC drops. At 16 MiB,
single-flow, incast, and aggregate all-to-all goodput were 329.44, 384.36, and 1413.55 Gbps.

These checks establish regression, network-integration and memory-safety coverage for the ns-3
model. They do not constitute formal UEC certification.

## Frozen comparison snapshot

On 2026-09-23 the existing 800 Gbps result artifacts were indexed in
`requirements/uec-baseline-800g.json` for later multi-protocol comparisons. No simulation or full
regression suite was run during that indexing step. The lightweight artifact checker and the
recorded headline metrics are documented in `baseline-freeze-800g.md`.
