# Compatibility and validation

## 0.1.5
On 3 November 2026, the packaged Windows dedicated server reproduced issue #1 on 0.1.4. The same two-floor test on 0.1.5 kept both floors powered after a 4 m Basic Wall covered the lower outlet and after the wall was removed. A separate outlet across an air gap stayed isolated. All 722 gameplay checks and four editor suites passed. The reporter also confirmed the fix in the original Windows Steam game on CL 513885 with SML 3.12.0.

## 0.1.4
Version 0.1.4, tested 28 October 2026 on Windows Steam, Satisfactory CL 513885, SML 3.12.0.

| Check | Result |
|---|---|
| Editor automation | Four suites passed |
| Stacking integration | 332 checks passed |
| Fresh-process stacking reload | 317 checks passed |
| Native vertical zooping | 84 checks passed |
| Combined 0.1.4 gameplay checks | 733 passed, zero failures |

Coverage includes five finishes, mixed heights, upper branches and machines, indicators, gaps/corner isolation, middle-block removal/replacement, save/reload, and upward/downward zoops at 0 and 37 degrees. Original inventory was preserved.

0.1.4 multiplayer/server/Epic gameplay and dense Designer suites were not rerun. Earlier versions had local client/server and Designer coverage; historical results are not fresh coverage for this version. Windows client and server packages built. Linux, WAN/multiple remote clients, Experimental, controllers and third-party finishes remain unverified or unavailable. Newer game builds are not implicitly certified.

See `validation/` for the 0.1.4 reports and a redacted historical validation record. Publishing source did not rebuild or retest gameplay. A repaired QA timer-lifetime failure occurred during initial testing; the final recorded runs passed.
