# Axiom 0.7 Phase 2 — legal filtering 후보 C2

## 결과 / 범위

**TYPE A `legal_fast_path`**를 구현했으며 기본값은 OFF다.

이 기능은 move ordering, pruning, evaluation, QSearch 정책을 변경하지 않고 **중복되는 legality board mutation을 제거**한다.

Full snapshot Undo와 기존 legal generator는 그대로 사용할 수 있다.

이 보고서는 첫 번째 primary-target candidate와 replication 작업을 다루며, **Phase 2 전체 63개 연구 항목이 모두 완료되었다는 의미는 아니다.**

관측 결과:

* legal-filter push: **-73.78%**
* total push: **-51.80%**
* attacked: **-50.79%**
* 동일 nodes에서 fixed-depth time: **-14.49%**

Reference metric은 혼합된 결과를 보였다.

* mean CP loss 개선
* Top-1 개선
* Top-3 감소

따라서 feature promotion은 하지 않는다.

**속도 향상만으로 playing strength가 향상됐다고 볼 수 없다.**

## Frozen identity 및 비교 기준 (items 1, 20–21)

| Artifact                                 | SHA256                                                           |
| ---------------------------------------- | ---------------------------------------------------------------- |
| A: baseline/v06-final.exe                | 6744B4A4851DBCF583F99BEDAB3CAE99C9363F41E5C7B24F6FF83EEB150B61E7 |
| B binary: baseline/v07-phase1.exe        | 7F404D751D30415BC70FC4644D308B4D34CAEA13738B21BF895AE171CF112982 |
| B source: baseline/v07-phase1-source.zip | 299F79CA087050EE66E66D5EB77F135AC050E0637A33E2A7200CE00080A65006 |
| C binary: baseline/v07-phase2.exe        | 79BF757B7911F6AB2B61E8F2CE929EB6260B0572CE3B3271F896C36CE4948800 |
| C source: baseline/v07-phase2-source.zip | 891B47E49D3303C96308943753CA33DC937354C7DE6CE1C7660D079C82E0AFDB |

B playing candidate는 `reuse_move_facts` ON을 의미한다.

C2는:

* `legal_fast_path` ON
* `reuse_move_facts` OFF

상태를 의미한다.

C2 benchmark는 **동일한 C binary**에서 legal OFF/ON을 비교한다.

C OFF와 frozen B binary OFF 간 fixed-node 비교는 100개 position에서 통과했다.

최종 contemporaneous A/B/C benchmark는 별도로 `results/v07p2-abc`에 기록되어 있다.

서로 다른 feature configuration과 과거 timing run을 혼합해서 해석해서는 안 된다.

현재 combined candidate는 구현되지 않았으며 추천하지도 않는다.

환경:

* VS 18 2026 Community
* MSVC 19.51.36246.0
* C++20
* Release AVX2 IPO
* Windows
* 6 logical processors

Git revision은 사용하지 않는다. Source ZIP identity를 revision 식별에 사용한다.

Archive는 절대로 overwrite하지 않는다.

최종 문서와 derived manifest generator는 candidate source snapshot 이후 작성되었으며, playing implementation 자체는 frozen 상태다.

이후 추가된 research-only legal-class instrumentation은:

`baseline/v07p2-attribution-source.zip`

에 있으며 SHA256은:

`01B64CC4B6CB3D972633BCB805C0D81370BA0561DBFB1DCE1A963EA22A0431CB`

이다.

해당 instrumentation은 production build에서는 compile out되며, 위의 playing binary는 변경되지 않았다.

## Legal fast path 및 정확성 (items 2–6, 29)

각 legal generation은 현재 king/check 상태를 계산한다.

현재 position이 check 상태이거나 king이 없거나 유효하지 않은 경우에는 기존 full generator로 fallback한다.

그 외의 경우에는 8개 king ray를 검사해:

* 하나의 friendly blocker
* 그 뒤에 있는 대응 가능한 enemy rook / bishop / queen

조합을 찾아 absolute pin을 식별한다.

이 정보는 immutable generation call 내부에서만 존재하며 push/pop 이후까지 cache되지 않는다.

Fast acceptance 조건:

* non-king
* unpinned
* non-EP

이다.

움직이지 않는 king은 일반적인 friendly non-king piece가 이동한다고 해서 knight/pawn/king에게 새롭게 공격받을 수 없다.

새로운 slider ray가 열리려면 기존의 유일한 blocker가 제거되어야 하며, pin detection이 이를 제외한다.

일반 capture는 destination occupancy를 대체한다.

Promotion은 destination piece를 변경하지만 관련 occupancy는 유지한다.

EP는 추가 square를 제거하므로 항상 slow path를 사용한다.

King move / capture / castling은 기존 push/check/pop semantics를 유지한다.

Pinned move는 무조건 reject하지 않고 실제로 legality를 검사한다.

`Board::legal_moves_reference()`는 기존 full-filter algorithm을 그대로 유지한다.

`Board::legal_fast_path`는 position identity가 아니라 execution policy다.

`run_single`은 worker마다 Limits에서 이 값을 명시적으로 설정한다.

Hash/history는 변경하지 않는다.

검증 결과:

* 1,000,000개의 **sampled position**, unique라고 주장하지 않음
* seed: `2026091307`
* sorted move-set이 아니라 exact ordered vector equality 비교
* recorded makes: 997,605
* 최대 160 plies의 sequence: 8,499개
* 모든 move는 전부 undo
* 복원 항목:

  * squares
  * FEN
  * side
  * castling rights
  * EP
  * clocks
  * hash
  * history
  * identities
  * proof_key
* king location은 별도 cache가 아니라 board square로 표현
* fixture:

  * castling
  * pinned EP
  * capture promotion
  * 모든 underpromotion
  * double check
  * absolute pins
  * mate
  * stalemate
  * 99-halfmove state
  * start perft4
* Start perft4 = **197,281**
* 기존 perft 및 rule audit는 계속 CTest에 포함
* 123 OFF/ON search:

  * bestmove
  * score/type
  * depth
  * nodes
  * PV
    모두 동일
* C OFF vs B OFF:

  * 100개의 exact fixed-node comparison 통과
* Release: 4/4, 8.89초
* Debug: 4/4, 48.29초
* ASan: 4/4, 101.85초
* research: 4/4
* UCI concurrent readiness 통과
* experimental multiworker stop 통과
* terminal test 통과
* trace determinism/filter/cap/parent test 통과

관측된 correctness mismatch는 없다.

다만 random sampling은 formal exhaustive proof가 아니다.

1,000,000개의 sample이 1,000,000개의 독립 게임을 의미하는 것도 아니다.

MSVC ASan 통과가 UBSan 통과를 의미하지 않는다.

Incremental Undo는 구현하지 않았다.

이번 primary-target candidate에서 이미 상당한 효과가 측정되었기 때문에 secondary candidate까지 구현할 필요는 없었다.

## Profile: 실제로 제거된 비용 (items 2, 4–5, 9)

동일한 5개 diagnostic FEN을 사용했다.

각각:

* 20k search nodes
* Threads1
* Hash32
* Middlegame
* ProofNodes0
* 시간 제한 없음
* research timing 활성화
* trace 비활성화

조건이다.

| Metric / 100k search nodes |       OFF |     C2 ON |    감소율 |
| -------------------------- | --------: | --------: | -----: |
| Legal-filter pushes        | 3,001,386 |   787,055 | 73.78% |
| Total pushes               | 4,274,388 | 2,060,057 | 51.80% |
| Pushes / node              |  42.74388 |  20.60057 | 51.80% |
| attacked calls             | 4,143,703 | 2,039,016 | 50.79% |
| SEE calls                  |    95,532 |    95,532 |     0% |

즉, 이 sample에서는 기존 legal-filter push 가운데 **2,214,331회가 불필요한 mutation이었다.**

남아 있는 slow check에는:

* king moves
* checked positions
* pins
* EP

등이 포함된다.

이들이 irreducible하다고 입증된 것은 아니다.

다른 경로에서는 여전히 full snapshot push/pop을 사용한다.

전체 attack call-site count는:

`results/v07p2-attacks/manifest.json`

에 기록되어 있다.

Research inclusive time에는 instrumentation overhead가 포함된다.

중첩된 `Legal` scope는 fast-path dispatch와 checked-position fallback을 모두 포함한다.

따라서 legal scope count는 **unique generation count가 아니다.**

또한 nested inclusive time을 서로 더해서는 안 된다.

Instrumented timing을 실제 wall-clock speedup claim으로 사용해서는 안 된다.

같은 5-FEN / 100k-node budget에서 follow-up research classification을 수행했다.

| Class        | Pseudo candidates |     Legal | Illegal | OFF pushes | ON pushes |
| ------------ | ----------------: | --------: | ------: | ---------: | --------: |
| Pawn         |           655,577 |   580,629 |  74,948 |    655,577 |    80,985 |
| Knight       |           407,205 |   368,873 |  38,332 |    407,205 |    41,808 |
| Bishop       |           308,973 |   277,742 |  31,231 |    308,973 |    34,085 |
| Rook         |           484,413 |   400,385 |  84,028 |    484,413 |    88,532 |
| Queen        |           660,665 |   608,971 |  51,694 |    660,665 |    57,092 |
| King         |           484,553 |   363,249 | 121,304 |    484,553 |   484,553 |
| Capture      |           241,822 |   217,139 |  24,683 |    241,822 |    51,491 |
| Quiet        |         2,759,564 | 2,382,710 | 376,854 |  2,759,564 |   735,564 |
| Promotion    |            26,828 |    24,408 |   2,420 |     26,828 |     2,420 |
| Castling     |            17,597 |    17,597 |       0 |     17,597 |    17,597 |
| EP           |               488 |       312 |     176 |        488 |       488 |
| In check     |           408,596 |    83,567 | 325,029 |    408,596 |   408,596 |
| Not in check |         2,592,790 | 2,516,282 |  76,508 |  2,592,790 |   378,459 |
| Pinned       |            24,338 |     6,651 |  17,687 |     24,338 |    24,338 |
| Unpinned     |         2,977,048 | 2,593,198 | 383,850 |  2,977,048 |   762,717 |

모든 category에서 OFF/ON 간 generated/legal/illegal total은 동일하다.

Category는 서로 겹친다.

따라서 서로 다른 dimension의 값을 합산해서는 안 된다.

이 환경에서는 per-move attacked call 수가 slow push 수와 동일했다.

Pseudo-generation attack query와 node setup은 특정 하나의 move에 귀속되지 않는다.

전체 per-class time 및 attack count는:

`results/v07p2-legal/classification.json`

에 있다.

Instrumented filtering time 예시:

* quiet: 540.90 → 193.27 ms
* king: 106.49 → 110.47 ms

이 값에는 observer overhead가 포함되어 있으며 pseudo generation/pin/check setup은 제외된다.

따라서 release speedup estimate로 사용해서는 안 된다.

Research classification build는:

* CTest 4/4
* UCI check
* trace check

를 통과했다.

## QSearch, Ordering, Safety 및 History (items 7–15)

100개의 balanced FEN, 각 20k nodes, C2 ON 조건:

| Qratio statistic |   Value |
| ---------------- | ------: |
| Median           | 88.505% |
| P75              | 91.385% |
| P90              | 96.155% |
| P95              | 97.265% |
| Maximum          | 99.085% |

37개 position이 90%를 초과했다.

해당 position의:

* FEN
* qply histogram
* check counter
* evasion counter

는:

`results/v07p2-qsearch/summary.json`

에 저장되어 있다.

Label은 모두 UNKNOWN이다.

높은 Qratio만으로는 필요한 tactical work인지 불필요한 wasted work인지 구분할 수 없다.

이전 5-position sample의 92.13% ratio는 이번 100-position corpus의 median을 대표하지 않았다.

동일 node/depth budget에서 C2는 qnode 수를 변경하지 않는다.

Tactical work를 의도적으로 제거하지 않는다.

QSearch는 별도의 Phase1 `reuse_move_facts` flag가 ON일 경우 이미 check/SEE reuse를 사용한다.

C2는 다음을 변경하지 않는다.

* SEE
* capture ordering
* delta threshold
* check
* promotion

다음은 아직 미완료 상태다.

* cutoff-index distribution
* QSearch chain attribution
* dedicated tactical holdout

임의적인 q-depth cap은 추가하지 않았다.

또한 다음도 **완료되지 않았다.**

* Safety consensus/weakening
* LMR false-negative sampling
* history saturation 및 calibration curve
* continuation overlap
* static-vs-search error classification
* eval term ablation
* high-confidence quiet-reference corpus

기존 0.6 evidence는 보존하지만, 이를 이용해 새로운 causal node saving이나 TYPE B benefit을 추정하지 않는다.

Exact ordered move-set preservation에는 quiet move도 포함되지만, 이것이 요청된 quiet tactical holdout을 대체하지는 않는다.

## 반복 Release Benchmark (items 16–19)

20개의 unique position을 사용했고 각 position을 3회 반복했다.

OFF/ON 순서는 번갈아 실행했다.

조건:

* same binary
* Threads1
* Hash32
* Middlegame
* proof disabled
* 의도적인 concurrent CPU-heavy workload 없음

Limit:

* 20k nodes
* depth4
* 100 ms

Raw result에는:

* moves
* scores
* PVs

가 포함되어 있다.

| Metric                                |                OFF |              C2 ON |
| ------------------------------------- | -----------------: | -----------------: |
| Fixed-node totals per repetition, ms  | 4528 / 4596 / 4783 | 3841 / 3808 / 3912 |
| Fixed-node median, ms                 |               4596 |     3841 (-16.43%) |
| Fixed-depth totals per repetition, ms | 3836 / 3680 / 4096 | 3132 / 3374 / 3280 |
| Fixed-depth median, ms                |               3836 |     3280 (-14.49%) |
| Fixed-depth nodes, all repetitions    |            913,128 |            913,128 |
| Fixed-depth qnodes                    |            842,187 |            842,187 |
| Fixed-time mean completed depth       |             3.2000 |             3.4833 |
| Fixed-time nodes, all repetitions     |            525,730 |  613,747 (+16.74%) |
| Fixed-time Top1, observations         |              12/60 |              15/60 |
| Fixed-time Top3, observations         |              33/60 |              30/60 |
| Mean CP loss, cp observations         |              39.63 |              34.74 |
| Loss >200cp                           |                  0 |                  0 |

Reference는:

* Stockfish 18 depth18 MultiPV3
* forced selected move search

를 사용했다.

반복 측정은 **20개 position이며 60개의 독립 position이 아니다.**

각 CP mean에서는 mate-valued observation 3개씩 제외했다.

추가적인:

* > 50cp
* > 100cp
* > 200cp
* > 400cp

count는:

`results/v07p2-fixed-time/tail-loss.json`

에 있다.

Mate miss/allowed는 아직 classification하지 않았다.

Depth20/22 stability confirmation 및 전체 critical/quiet holdout은 아직 수행하지 않았다.

이번 결과는 throughput gain이지 node allocation 개선이 아니다.

같은 depth에서는 search tree가 동일하다.

Top3가 감소했기 때문에 reference non-regression이 확립되지 않았다.

따라서 C2도 `reuse_move_facts`도 promotion하지 않는다.

두 기능의 gain을 단순 합산할 수도 없다.

Fixed-time에서 실제 decision이 달라진 unique position은 2개였다.

세 번의 repetition 모두 동일한 결과를 보였다.

첫 번째는 benchmark index3이다.

* OFF: depth2, `g1h1`
* C2: depth3, `c4c5`

Depth18 forced reference score:

* `c4c5`: -274
* `g1h1`: -406

즉 **132cp 회복**했다.

두 번째는 index8이다.

* OFF: depth3, `b1c3`
* C2: depth4, `c1f4`

Forced reference score:

* `c1f4`: -10
* `b1c3`: +29

즉 **39cp 악화**됐다.

이 사례들은 depth-transition holdout case로는 유용하지만, 특정 LMR/evaluation cause를 입증하는 것은 아니다.

관련 FEN과 observation은:

`results/v07p2-fixed-time/changed-decisions.json`

에 있다.

## Playing Test / Replication (items 1, 22–28)

완료된 C2 vs frozen A 결과:

| Run                          | W / L / D    | Score | Runner Elo ± interval half-width |    LOS |   LLR |
| ---------------------------- | ------------ | ----: | -------------------------------- | -----: | ----: |
| 20-game smoke, 1+0.01        | 7 / 9 / 4    |   45% | -34.86 ±144.80                   | 30.74% | -0.04 |
| 100-game pilot, 1+0.01       | 37 / 46 / 17 | 45.5% | -31.35 ±62.94                    | 16.04% | -0.17 |
| 20-game medium smoke, 5+0.05 | 7 / 11 / 2   |   40% | -70.44 ±158.55                   | 16.74% | -0.07 |

Pilot의 nominal runner interval:

**[-94.29, +31.59]**

은 0을 포함한다.

모든 SPRT는 **INCONCLUSIVE**다.

현재 결과는 C2보다 reference 쪽에 유리하다.

모든 side/run에서:

* time forfeit: 0
* illegal move: 0

이다.

Medium test는 smoke 규모에 불과하며 long-TC strength의 증거가 아니다.

반복 사용된 opening을 여러 run에서 합쳐 pooled estimate를 만들지 않았다.

**C2는 playing-strength promotion 조건을 통과하지 못했으며 계속 OFF 상태로 유지한다.**

독립적인 Phase1 B reuse replication도 frozen A를 상대로 완료했다.

총 300게임:

* 115승
* 123패
* 62무

Score:

**48.67%**

Runner Elo:

**-9.27 ±35.15**

Nominal interval:

**[-44.42, +25.88]**

LOS:

**30.20%**

LLR:

**-0.18**

결론:

**SPRT INCONCLUSIVE**

양쪽 모두:

* time forfeit 0
* illegal move 0

이다.

이전에 관측된 **100-game 63% 결과는 재현되지 않았다.**

따라서 그 결과는 이전 pilot observation일 뿐이며, 확립된 +92 Elo 성능 향상으로 볼 수 없다.

`reuse_move_facts` 역시 계속 OFF 상태로 유지한다.

모든 대국은 다음 조건을 사용한다.

* Threads1
* Hash32
* ProofNodes0
* matched Middlegame features
* reversed colors
* concurrency2

Replication opening은 Phase1과 exact FEN overlap이 0인 새로운 20개의 legal position을 사용했다.

또한:

* reversed source ordering
* new seed `2026091302`

를 사용했다.

단, opening family 자체는 겹칠 수 있다.

이전 Phase1 100게임 결과를 replication 데이터와 합쳐서는 안 된다.

SPRT 설정:

* H0 = 0 Elo
* H1 = 5 Elo
* alpha = .05
* beta = .05
* bounds = ±2.94

LLR은 runner Elo, interval, LOS와 별도로 보고한다.

Pilot 결과만으로 default-on이나 Elo gain을 주장하지 않는다.

## 남은 작업 / 다음 Candidate (items 29–30)

Legal fast-path의 핵심 correctness는 현재 측정된 gate를 통과했다.

하지만 broader tactical quality와 decisive strength evidence는 아직 남아 있다.

다음으로 가장 가치가 높은 작업은:

1. mixed fixed-time position 추가 분석
2. independent quiet/tactical holdout
3. 측정된 piece-class attribution 활용

순이다.

실망스러운 pilot 결과에 대응한다고 해서 Safety/LMR retuning을 C2에 섞어서는 안 된다.

현재 legal mutation이 이미 73.78% 감소한 만큼 incremental Undo는 secondary priority다.

TYPE B experiment는 계속 별도로 유지한다.

요청된 모든 result directory에는 실제 evidence 또는 명시적인 deferred status가 존재한다.

Directory가 존재한다는 것 자체가 해당 experiment가 실제 수행되었다는 의미는 아니다.

## 재현 방법

```powershell
.\build.ps1 -Configuration Release -NativeAVX2
.\build\Release\axiom_legal_fast.exe 1000000
node tests/move-facts.mjs build/Release/axiom-0.7-cost.exe results/new-legal-equivalence.json legal_fast_path
node tools/v07-benchmark.mjs build/Release/axiom-0.7-cost.exe results/new-legal-bench legal_fast_path
```

CLI candidate:

`--middlegame --feature legal_fast_path`

UCI candidate:

`Middlegame=true`

`Feature_legal_fast_path=true`

기본값은 계속 OFF다.

`Experimental=true`는 unrelated experimental/rejected flag까지 활성화하므로 권장 playing preset이 아니다.

Result path는 overwrite를 허용하지 않는다.
