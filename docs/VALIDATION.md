# Compatibility and validation

Version 0.1.4, tested 28 October 2026 on Windows Steam, Satisfactory CL 513885, SML 3.12.0.

| Check | Result |
|---|---|
| Editor automation | Four suites passed |
| Stacking integration | 332 checks passed |
| Fresh-process stacking reload | 317 checks passed |
| Native vertical zooping | 84 checks passed |
| Latest combined gameplay checks | 733 passed, zero failures |

Coverage includes five finishes, mixed heights, upper branches and machines, indicators, gaps/corner isolation, middle-block removal/replacement, save/reload, and upward/downward zoops at 0 and 37 degrees. Original inventory was preserved.

0.1.4 multiplayer/server/Epic gameplay and dense Designer suites were not rerun. Earlier versions had local client/server and Designer coverage; historical results are not fresh coverage for this version. Windows client and server packages built. Linux, WAN/multiple remote clients, Experimental, controllers and third-party finishes remain unverified or unavailable. Newer game builds are not implicitly certified.

See `validation/` for the latest reports and a redacted historical validation record. Publishing source did not rebuild or retest gameplay. A repaired QA timer-lifetime failure occurred during initial testing; the final recorded runs passed.
