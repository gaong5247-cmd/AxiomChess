# Axiom 0.7 — 측정 기반 비용 절감, 첫 번째 후보

## 결과 및 범위

책임 분리, 컴파일 타임 연구용 비용 프로파일링, 그리고 하나의 독립적인 최적화를 구현했다. 해당 최적화는 **move ordering 과정에서 이미 정확하게 계산된 check/SEE 정보를 재사용**하는 것이다.

`reuse_move_facts`는 여전히 **CANDIDATE / 기본값 OFF** 상태다. 새로운 pruning, evaluation 재튜닝, safety-mask 변경, board 구조 재작성, feature 승격은 수행하지 않았다.

이번 작업은 첫 번째 후보 실험을 완료한 것이며, **0.7 로드맵의 모든 항목이 완료되었다는 의미도 아니고 Elo 향상이 입증되었다는 의미도 아니다.**

Production 빌드:

`build/Release/axiom-0.7-cost.exe`

Research 빌드:

`build-causal/Release/axiom-0.7-cost-research.exe`

환경은 Visual Studio 18 2026, MSVC 19.51.36246.0, C++20, Release AVX2/IPO, Windows, 6 logical processors다.

소스는 Git checkout 상태가 아니며, frozen ZIP의 hash를 통해 revision을 식별한다.

| Frozen artifact                   | SHA256                                                           |
| --------------------------------- | ---------------------------------------------------------------- |
| baseline/v06-final.exe            | 6744B4A4851DBCF583F99BEDAB3CAE99C9363F41E5C7B24F6FF83EEB150B61E7 |
| baseline/v06-final-source.zip     | D8DF18F7663BE41D0FE96331A2614D3056EE9F09345EE23187C9E3E1B3FDB108 |
| baseline/v07-candidate.exe        | 7f404d751d30415bc70fc4644d308b4d34caea13738b21bf895ae171cf112982 |
| baseline/v07-candidate-source.zip | 299f79ca087050ee66e66d5eb77f135ac050e0637a33e2a7200ce00080a65006 |

기존 바이너리, 소스 아카이브, 실패한 실험 및 모든 0.6 결과는 그대로 보존했다. Candidate archive에는 구현 코드, 테스트, 도구가 포함되어 있으며 이 최종 보고서는 그 이후 작성되었다.

각 실험의 manifest에는 실제 binary hash, command, option, budget, corpus, reference, date가 기록되어 있다. Derived manifest는 원래 측정값을 가리키며, 요청된 모든 측정이 존재한다는 의미는 아니다.

## 1–4. 주요 비용, Board, Legal Generation, Attack

Research profile 조건은 `tests/bench.fens`의 5개 포지션, 각각 20,000 nodes, Threads 1, Hash 32 MB, Middlegame, ProofNodes 0, 시간 제한 없음, trace 비활성화다.

**Instrumented time에는 observer overhead가 포함되어 있다.** 따라서 성능에 관한 주장은 아래의 반복 Release benchmark를 기준으로 해야 한다.

재귀적으로 중첩된 inclusive time은 서로 합산해서는 안 된다.

Candidate OFF 상태에서 exclusive instrumented time 상위 10개:

| Scope                                      |     ms |
| ------------------------------------------ | -----: |
| Legal generation, instrumented children 제외 | 260.95 |
| attacked                                   | 254.84 |
| push                                       | 234.78 |
| QSearch, instrumented children 제외          | 136.74 |
| gives_check, instrumented children 제외      | 115.66 |
| evaluation, instrumented children 제외       | 112.72 |
| pop                                        | 112.05 |
| attack map                                 |  96.47 |
| legal captures                             |  83.57 |
| pseudo generation                          |  71.92 |

Board에서는 총 4,274,388회의 push와 4,274,429회의 pop이 발생했으며, 추가로 41회의 null push가 있었다.

검색된 node 하나당 push는 42.74388회 발생했다.

각 snapshot은 논리적으로 512-byte 크기의 square array를 복사하며, 총 계산된 payload는 4,377,015,296 bytes다. 이는 **실제로 측정된 DRAM traffic을 의미하지 않는다.**

Push 발생 원인:

* legal filtering: 3,001,386
* gives_check: 741,843
* SEE: 313,955
* QSearch: 186,952
* main search: 30,252

Undo는 기존의 full snapshot 방식을 그대로 유지한다.

Legal generation은 109,644회 호출되었으며 inclusive time은 695.12 ms다. Pseudo generation의 inclusive time은 78.67 ms다.

`attacked`는 4,143,703회 호출되었고 254.84 ms가 소요됐다.

전체 origin distribution은 `results/v07-legalgen/manifest.json`과 profile summary에 기록되어 있다.

Pin detection, special move check, evasion filtering에는 아직 별도의 timer를 적용하지 않았다.

## 5–9. QSearch, SEE, gives_check 및 Ordering

Profile에서 qnodes는 92,132 / 100,000으로 **92.13%**다.

Reuse를 활성화해도 search tree와 qnode 수는 변하지 않았다.

* q_searched: 69,775
* checks: 18,461
* evasions: 25,299
* stand-pat cutoffs: 37,566
* SEE skips: 3,438
* delta skips: 5,574

`q_generated`는 capture-only candidate가 아니라 **생성된 모든 legal candidate**를 센다.

Histogram은 각 position별로 보존했다.

5개의 FEN 중 3개가 0.90 screening threshold를 초과하지만, 원인은 모두 **UNKNOWN**이며 실제 explosion이 입증된 것은 아니다.

임의적인 q-depth cap이나 새로운 delta rule은 추가하지 않았다.

| Operation   | OFF calls |  ON calls |  Change |
| ----------- | --------: | --------: | ------: |
| push        | 4,274,388 | 4,102,804 |  -4.01% |
| gives_check |   741,843 |   608,807 | -17.94% |
| SEE         |    95,532 |    85,533 | -10.47% |

OFF 상태에서 SEE inclusive time은 144.99 ms, gives_check는 201.76 ms다.

SEE call origin과 내부 push/attack 비용은 보존했지만, 평균 recursive depth는 **측정하지 않았다.**

기존의 legal mutation-based SEE를 reference 및 pruning implementation으로 유지한다.

Ordering의 inclusive time은 271.16 ms이며, scoring은 258.28 ms다. 여기에는 check/SEE가 포함된다. Sort 자체는 7.03 ms에 불과했다.

따라서 이 표본에서는 sort가 주요 비용이 아니다.

Staged picker 또는 history bucket rewrite를 정당화할 만한 결과는 없었으며, 이전 partial ordering 역시 OFF 상태로 유지한다.

## 10–14. Safety, Evaluation, Pawn Cache 및 TT

Safety policy는 변경하지 않았다.

보존된 0.6의 300-FEN / 20k-node / 9-mask 연구 결과:

* ALL mean CP loss: 71.65
* no-improving: 68.49
* no-PV: 68.19

이들은 잠정적인 overprotection candidate일 뿐이며, causal node-cost estimate 또는 feature promotion의 증거가 아니다.

모든 `no-*` mask는 **legacy가 아니라 ALL에서 bit를 제거한다.**

Joint activation credit은 독립적인 benefit을 의미하지 않는다.

Pair interaction trace는 연구 증거로 유지하지만, 각 reason에 대한 counterfactual cost/benefit, reference saves/harms 및 mask 변경에 대한 paired test는 아직 완료되지 않았다.

이번 candidate에서는 safety reason을 약화하지 않았다.

Evaluation inclusive time은 327.19 ms다.

* attack map: 96.47 ms
* king terms: 46.68 ms
* pawn probe: 72.42 ms inclusive
* pawn compute: 50.76 ms

Pawn hit/miss:

* hits: 74,458
* misses: 28,164
* hit rate: **72.56%**

Replacement/collision/entry-layout 측정과 개별 material/PST/mobility/space/phase timing은 아직 완료되지 않았다.

예약된 `strategic_eval`, `phase`, `pawn_terms` profile counter는 아직 instrument되지 않았다. 따라서 이들의 0 값은 **비용이 없다는 뜻이 아니라 측정되지 않았다는 뜻이다.**

상관관계가 있는 evaluation term을 제거하지 않았다.

Correction holdout은 이전의 작은 개선인 **93.7167 → 93.5967 cp MAE** 결과를 그대로 유지하며, 새로운 retuning은 수행하지 않았다.

TT:

* probes: 7,841
* hits: 2,249
* proof-key construction: 7,911 calls / 6.64 ms

Proof-key timing은 context comparison timing이 아니다.

TT entry size/bandwidth 및 compact-context experiment는 아직 완료되지 않았으며 strict identity guard를 유지한다.

## 15–16. Architecture 및 결정 사항

`qsearch.cpp`, `move_order.cpp`, `search_report.cpp`가 각각 자신의 책임을 담당하도록 분리했다.

공유되는 내부 RAII move restoration과 interruption type을 통해 파일 간 unwinding 동작을 일관되게 유지한다.

Main PVS/iteration/history는 worker-owned Search method로 유지했다.

Virtual dispatch나 새로운 context pointer bundle은 도입하지 않았다.

Research event와 상세 timer는 compile-time에서 production 코드와 분리된다.

Candidate의 per-ply fact는 sorting 이후에도 원래의 정확한 move ordinal을 보존한다.

기존에 계산된 check 결과와 이미 알려진 capture SEE 값을 재사용한다.

알 수 없는 TT-move SEE는 기존처럼 정상적으로 계산한다.

Scratch 공간은 기능이 활성화됐을 때 worker당 한 번만 allocation하며, move list가 지나치게 크면 기존 방식으로 fallback한다.

Same-ply auxiliary search는 ordering보다 먼저 수행된다.

Position을 넘어선 caching, approximation, move reclassification은 하지 않는다.

새로운 optimization이 명백히 해롭다고 판정되어 reject된 것은 없다.

다만 다음 항목들은 **rejected가 아니라 deferred** 상태다.

* fast SEE
* direct legal generation
* incremental Undo
* staged picker
* compact TT

`rfp`, `calibrated_lmr`, `adaptive_time`은 계속 OFF 상태이며 기존 실패 증거 역시 보존한다.

## 17–20. 반복 Benchmark 및 Reference 품질

20개의 unique FEN을 사용했다.

구성은 bench 5개 + critical corpus의 첫 15개다.

각각 3회 반복하고 OFF/ON 순서를 번갈아 사용했다.

동일한 Release binary, Threads 1, Hash 32, proof disabled 조건이다.

의도적인 concurrent benchmark workload는 사용하지 않았다.

제한:

* 20k nodes
* depth 4
* 100 ms

Reference는 Stockfish 18 depth 18 MultiPV 3와 선택된 move에 대한 forced search를 사용했다.

| Mode                               |     OFF |      ON | 해석                  |
| ---------------------------------- | ------: | ------: | ------------------- |
| Fixed-node median time / 20 FEN    | 4741 ms | 4662 ms | 1.67% 감소            |
| Fixed-depth median time / 20 FEN   | 3929 ms | 3786 ms | 3.64% 감소            |
| Fixed-depth nodes / all repeats    | 913,128 | 913,128 | search tree work 동일 |
| Fixed-time mean completed depth    |  3.2333 |  3.2500 | 작은 차이               |
| Fixed-time nodes / all repeats     | 515,674 | 533,898 | +3.53%              |
| Fixed-time Top-1 / 60 observations |      12 |      12 | 변화 없음               |
| Fixed-time Top-3 / 60 observations |      33 |      33 | 변화 없음               |
| Mean CP loss / 57 cp observations  |   39.63 |   39.63 | 변화 없음               |
| Blunders >200cp                    |       0 |       0 | 작은 표본에 한함           |

반복 observation은 **60개의 독립 position이 아니라 20개의 독립 position**이다.

Mate value가 포함된 row는 CP 평균 계산에서 제외했다.

Reference depth 20/22 안정성 확인은 아직 수행하지 않았다.

Exact fixed-node OFF/ON 비교는 castling, promotion, pinned EP를 포함해 **123/123 일치**했다.

Frozen 0.6과 0.7 OFF 비교에서는 **100/100 position에서 move/depth/nodes/score/PV가 일치**했다.

Refactor-only stage 역시 별도로 100 position을 통과했다.

Fixed-node/depth의 변화는 throughput 개선을 의미할 뿐, search-tree 크기가 감소했거나 실제 strength가 입증됐다는 뜻은 아니다.

## 21–24. Paired Games, Elo, SPRT 및 Time Safety

완료된 대국 결과:

* Smoke: **9승 / 6패 / 5무**, 총 20게임
* Pilot: **56승 / 30패 / 14무**, 총 100게임, score 63%

양쪽 모두 두 실험에서 **time forfeit 0회, illegal move 0회**를 기록했다.

자세한 결과는 `results/v07-paired-smoke` 및 `results/v07-paired`에 있다.

Runner가 계산한 pilot Elo 추정치는:

**+92.46 ±65.95**

명목상 interval:

**[+26.51, +158.41]**

LOS:

**99.83%**

그러나 이 runner interval은 opening reuse, 짧은 TC, experiment-selection 등의 모든 제한을 반영하지 않는다.

**SPRT LLR은 +0.42**, 경계는 **[-2.94, +2.94]**로 아직 결론이 나지 않았다.

측정된 throughput 향상에 비해 pilot Elo 추정치가 매우 크기 때문에 feature promotion이 아니라 **독립적인 replication이 필요하다.**

Smoke와 pilot은 opening/seed를 재사용하므로 서로 독립적인 evidence로 합산하지 않았다.

따라서 **확립된 Elo 향상이나 feature promotion을 주장하지 않는다.**

두 실험 모두 frozen 0.6을 baseline으로 사용했다.

동일한 20개 opening을 color reversed 방식으로 사용했으며:

* Threads 1
* Hash 32
* ProofNodes 0
* TC 1+0.01
* concurrency 2
* Middlegame features

조건이다.

Candidate에서만 `reuse_move_facts`를 활성화했다.

짧은 TC는 명확한 제한 사항이며 long-TC strength에 관한 주장은 하지 않는다.

External stop probe는 10회의 search에서 OFF/ON 모두 request를 100 ms 후 전달했다.

관측된 bestmove 반환까지의 최대 시간은 **0.64 ms**였다.

여기에는 IPC/scheduling이 포함되며 subsystem은 UNKNOWN이다. 따라서 worst-case bound로 볼 수 없다.

Fixed-time benchmark에서는 hard overshoot가 0회였다.

TimeGuard와 adaptive time 설정은 변경하지 않았으며 time-management retuning도 수행하지 않았다.

## 25–27. Correctness, 미해결 위험 및 다음 작업

검증 결과:

* Release CTest: 3/3, 5.90초
* Debug CTest: 3/3, 46.10초
* MSVC ASan: 3/3, 94.16초
* Research Release: 3/3

Cached path는 calibration test에 명시적으로 포함되어 있다.

UCI smoke test는 다음을 포함한다.

* finite search
* concurrent readiness
* multiworker experimental search
* stop
* terminal position
* quit

다음 테스트도 통과했다.

* trace-on/off determinism
* parent IDs
* filter/cap/overwrite protection
* safety-mask JSON
* forced-root replay
* correction read-only state
* ordering uniqueness

관측된 regression은 없다.

MSVC ASan은 UBSan이 아니다.

초기 shell invocation에서 `build.ps1` 실행 시 `.\`가 누락되었으나 수정했다.

수정된 Debug/Release 실행 기록은 `*-validation-final.log`에 있으며, 완료된 ASan log는 해당 shell invocation 오류와 독립적이다.

아직 완료되지 않은 항목:

* massive board make/unmake fuzz — Undo 변경이 없으므로 아직 수행하지 않음
* 새로운 legal/SEE equivalence implementation
* 1000개 이상의 critical corpus
* complete subsystem profile
* SEE recursion-depth distribution
* full causal qsearch classification
* safety benefit/node-cost attribution
* long-TC paired testing
* decisive SPRT
* depth 20/22 reference stabilization

Fresh subtree replay는 기존 TT/history/stack을 복원하지 않으므로 그 진단 결과를 causal proof라고 표현해서는 안 된다.

다음으로 가장 가치가 높은 작업은 **현재 unchanged-tree candidate에 대해 fixed-time 및 paired evidence를 확대하는 것**이다.

그다음 full correctness fixture를 유지하면서 legal-filter push cost를 독립적으로 분리해 측정한다.

Direct legal generation 또는 incremental Undo 중 무엇을 선택할지 결정하기 전에 fine-grained profile과 check-chain diagnostics를 더 정교하게 만들어야 한다.

Optimization 변경은 서로 분리해서 진행한다.

## 재현 방법

PowerShell에서 프로젝트 디렉터리 기준으로 실행한다.

결과 디렉터리는 실행 전에 존재하지 않아야 한다.

```powershell
.\build.ps1 -Configuration Release -NativeAVX2
.\tools\build-causal.ps1
node tests/move-facts.mjs build/Release/axiom-0.7-cost.exe results/new-equivalence.json
node tools/v07-benchmark.mjs build/Release/axiom-0.7-cost.exe results/new-benchmark
node tools/v07-profile.mjs build-causal/Release/axiom-0.7-cost-research.exe results/new-profile --feature reuse_move_facts
```

Playing baseline에서는 UCI 옵션 `Middlegame=true`를 사용한다.

독립적인 candidate를 활성화하려면 추가로:

`Feature_reuse_move_facts=true`

를 사용한다.

`Experimental=true`는 다른 rejected feature까지 활성화하므로 실제 playing recommendation이 아니다.

상세 artifact는 요청된 `v07-*` 디렉터리에 있으며, `PARTIAL` / `UNMEASURED` 상태는 의도된 것이다.
