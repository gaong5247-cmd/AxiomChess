# Axiom Chess 0.5 — Search Calibration

작업일: 2026-09-12. 기반: Axiom 0.4 audit. C++20 / Visual Studio 18 Community.

이번 버전은 **측정 가능한 calibration 기반과 실험 후보**입니다. 기능 수, NPS, 노드 감소를 Elo 증가로 바꾸어 주장하지 않습니다. 기존 0.3/0.4 실행 파일과 과거 결과는 보존했습니다. 기본/Middlegame 프리셋은 그대로이며, 후보 3개는 기본 OFF입니다.

## 1. 변경 파일과 baseline

- `include/axiom/calibration.hpp`: worker-local 계측, 선택적 inclusive timer, bounded eval trend.
- `include/axiom/engine.hpp`, `src/search.cpp`: 계측, pre-update correction 표본, LMR forensic 표본, root 이력, 후보 safety/time 정책.
- `src/feedback.cpp`: 기존 5개 correction 성분 노출 및 mask, 후보 Feature 등록. correction 알고리즘을 중복 구현하지 않음.
- `src/main.cpp`: 0.5 UCI 이름, CLI `--profile`, `--calibration-samples`, `--correction-mask`.
- `include/axiom/san.hpp`, `src/pgn.cpp`: 합법 수로 검증하는 SAN/mainline PGN → FEN 변환.
- `tests/calibration.cpp`, `tests/audit.cpp`, `tests/calibration-tools.mjs`, `tests/audit-equivalence.mjs`: 신규 검사 및 비교 경로 인수화. 기존 1,569개 체크 및 audit는 삭제·약화하지 않음.
- `tools/calibrate.mjs`, `critical-corpus.mjs`, `lmr-forensics.mjs`, `calibration-openings.mjs`, `calibration-deep.ps1`, `match-summary.mjs`: 측정/ablation/interaction/corpus/forensic/대국 집계 도구.
- `tools/run-sprt.ps1`: 새 후보 Feature 허용. 기존 runner와 바이너리 동결 재사용.
- `tools/sanitize.ps1`, `CMakeLists.txt`, `build.ps1`: 0.5 출력명, 새 테스트/PGN 타깃, MSVC ASan 구성.
- `README.md`, 이 보고서: 최신 경로, 사용법, 측정 결과 및 비승격 결정.

동결 baseline: `baseline/calibration-v04.exe`, `baseline/calibration-v04-source.zip`. 단계별 `baseline/v05-*.exe`와 `results/v05-phase-*.log`를 보존했습니다. Phase B만 적용한 5-FEN 비교에서 기존 0.4와 move/depth/nodes/score가 모두 같았습니다 (`v05-instrumentation-bench.json`).

## 2. 실제 탐색 경로 지도

| 기능 | 영향 경로 | 구현 위치 |
|---|---|---|
| PVS, iterative deepening, aspiration | root/recursive window, 재검색 | `Search::root`, `negamax`, `run_single` |
| TT, mate normalization, replacement | probe/cutoff/order/store | `negamax`, `TranspositionTable` |
| Qsearch, SEE | horizon/교환 평가/가지치기/수 정렬 | `quiescence`, `order`, `see` |
| Correction | static eval, pruning 판단, 후속 학습 | `corrected_eval`, negamax 종료 |
| Continuation, capture, countermove | move ordering, cutoff history | `history_score`, `order`, `update_move` |
| Dynamic LMR, history/SEE pruning | reduction/skip/full-depth 재검증 | negamax move loop |
| Singular | excluded auxiliary search, 1-ply extension | negamax TT candidate |
| Verified null | synthetic null + 실제 위치 검증 | negamax null branch |
| ProbCut, RFP | qsearch/감축 탐색/얕은 non-PV cutoff | negamax pre-loop |
| IID | depth-2 auxiliary seed, ordering | negamax TT move 부재 |
| 전략 평가, king safety, pawn cache | eval/safety/cache | `evaluate_score`, `strategic_candidate` |
| Mate distance, proof, verification, TB | bounds/독립 증명/결과 분류 | negamax, `run_single`, `MateSolver`, `Tablebase` |
| Time management | UCI clock allocation, soft stop | `uci`, `run_single` |
| Search Safety | pruning/reduction 보호 | `SafetySignals`, negamax/qsearch |

## 3. 추가 correctness 이슈

이번 단계에서 0.4의 새로운 게임 규칙 오류를 입증하지는 않았습니다. 그러나 짧은 시간 제한의 대국에서 **시간 예산 처리 문제**를 발견했습니다. 기존 시계는 `go`를 받은 후 검색 스레드가 시작할 때까지의 대기 시간을 차감하지 않았고, 최초 fallback 후보 평가에도 deadline 검사가 없었습니다. 최종 0.5는 queue 지연을 검색 예산에서 차감하고 fallback 평가도 deadline에서 중단합니다. Adaptive-time 후보의 남은 시계 여유는 25ms에서 `max(75ms, remaining/20)`로 강화했습니다. OS 지연 전체를 제어할 수 있다는 보장은 아닙니다.

신규 계측 개발 중 depth가 얕아 실제 reduction이 0인데 reduction 이벤트를 세는 문제를 고쳤고, 분석 도구가 threads/root-correction을 일반 이벤트처럼 합산하던 문제도 수정했습니다. 초기 summary의 `stats.threads`는 위치 수를 합산한 값이므로 사용하지 말고 manifest의 실제 1 thread 설정을 사용해야 합니다. 원본 파일은 보존했습니다.

기존 0.4의 PV-after-threefold 경고 가능성은 남습니다. FEN은 전체 반복 이력을 표현하지 않으므로 corpus 추출에서는 반복이 관측된 위치를 제외합니다. 이것만으로 모든 이후 반복 가능성이 동일해지는 것은 아닙니다.

## 4. Correction History 품질

기존 성분은 **Pawn / Material counts / Minor placement / Non-pawn placement / Previous-move token**입니다. Major-piece 전용 성분을 새로 추가하지 않았습니다. 각 성분은 독립 int16 bounded EMA, 값 ±4096(16배 스케일), 합산 보정 ±256cp를 유지합니다.

`--correction-mask 0..31`로 성분별 비교가 가능합니다. 기본 31은 기존 수식과 동일합니다. 예: 30은 pawn 성분 제외. 선택된 성분 개수로 평균하므로 이를 실제 A/B로 해석하며, 단순히 동일 검색에서 한 항만 뺀 결과라고 부르지 않습니다.

20 FEN × 15,000 nodes, 성분 제외 5개 + 전체 1개 = 120회 탐색 (`v05-correction-quality`):

| 278개 pre-update 표본 | Raw | Corrected |
|---|---:|---:|
| MAE | 36.96cp | 33.07cp |
| Median absolute error | 28cp | 23.5cp |
| P90 | 78cp | 73cp |
| Sign agreement | 100% | 100% |

모두 기물 수 >10인 표본이어서 이 실행의 endgame-only 지표는 **null / 표본 없음**입니다. 원래 score의 부호가 치우친 표본이므로 100% sign agreement는 유용한 성능 증거가 아닙니다. 목표값은 **유한 검색 점수**이며 Stockfish truth가 아닙니다. 같은 검색의 표본 선택 편향도 있습니다. mate/proven/TB/check/synthetic/auxiliary 결과는 이 학습/측정 표본에 사용하지 않습니다. forced quiet tactical line을 완벽히 검출하는 것은 아닙니다.

별도 5-FEN profile 실행에서 얻은 endgame 12개 표본은 MAE 34.08→32.08cp, median 37→33.5cp, P90 64→63cp, sign agreement 75%→75%였습니다. 다른 위치 집합의 극소수 표본이므로 위 278개 측정과 합쳐 성능을 부풀리지 않습니다.

성분 제외 시 corrected MAE는 32.59~34.74cp였지만 검색 표본이 서로 달라졌습니다. 독립 holdout 검증 없이 성분 제거/가중치 승격을 하지 않았습니다. 동일 표본의 component-only 지표도 JSON에 별도 보존했습니다.

## 5. LMR / improving-worsening / history 안전성

`Feature_trend_safety`: 같은 side의 2ply 전 corrected eval과 비교한 trend를 ±256으로 clamp합니다. check/missing stack은 중립입니다. |trend|≥80이면 불확실한 노드로 보호하며, 여러 pruning에 aggression 보너스를 중복 가산하지 않습니다.

`Feature_calibrated_lmr`: 기존 clamp 및 reduced fail-high의 full-depth 재검색을 유지합니다. Countermove/높은 continuation/불안정 eval/단 하나의 합법 수를 보호합니다. 낮은 history로 삭제하던 quiet move는 이 후보에서 검색으로 전환합니다(깊이가 허용되면 reduction, 이후 fail-high 재검색). 별도 IIR은 추가하지 않았습니다.

Critical 11 FEN × 20,000 nodes에서 전체 기준 LMR 570회 → trend 후보 524회 → calibrated LMR 후보 431회였습니다. 이 작은 표본의 LMR 재검색은 0회였으므로 재검색 품질은 별도 깊은 fixture로 검증합니다. 후보 LMR은 참조 bestmove 일치 **7/11 → 6/11**, 평균 depth 4.00 → 3.91로 나빠져 **기본 OFF를 유지**합니다.

`--calibration-samples N`은 최대 4096개의 pre-update/LMR 표본을 별도로 제한합니다. LMR 기록에는 FEN/root branch/move/depth/reduced depth/index/history/continuation/capture history/eval/trend/reduced score/research/final score가 있습니다. `lmr-forensics.mjs`는 참조 root branch가 실제로 감축됐고 최종 선택에서 탈락했는지 조사합니다. 표본 누락과 검색 창 차이가 있으므로 suspect이지 잘못된 pruning의 증명이 아닙니다. 루트 자체는 감축하지 않으므로 해당 루트 아래 branch를 검사합니다.

## 6. Root / Time Management

완료된 iterative-deepening마다 root move의 score/previous score/depth/누적 nodes/PV/stable iterations/Welford variance를 유지합니다. `root_history`, `root_node_fraction`, `time_factor`로 출력합니다. 실패한 aspiration 시도의 노드는 root history에 포함되지 않으며 전체 `nodes`에는 포함됩니다. 후보 score에 bound 값도 섞이므로 variance는 휴리스틱 신호입니다.

`Feature_adaptive_time`은 기존 `Feature_time_management`와 함께 사용합니다. best/PV/eval 안정, 후보 gap, aspiration 실패, root node 집중도, 합법 수 개수를 사용해 soft factor 0.5~1.5를 선택합니다. hard deadline을 늘리지 않습니다. `--time`만 지정한 분석에는 soft limit가 없으므로 `--soft-time`도 지정해야 이 정책이 발동합니다. 일반 OS scheduling/비선점 make-unmake 등의 비용 때문에 벽시계 초과를 수학적으로 0ms 보장하지는 않습니다. 테스트는 scheduling 허용 오차를 명시합니다.

## 7. Singular / Null / ProbCut / IID / RFP

기존 알고리즘은 유지하고 실제 비용을 계측했습니다. Singular excluded nodes, extension branch nodes/beta cutoffs/PV updates; null fail-high/verification success/rejection/nodes/material rejection; ProbCut qsuccess/reduced success/nodes; IID nodes/seed move 발견; RFP attempts/cuts가 추가됐습니다.

비용 카운터는 **완료된 호출** 기준이며 중단된 호출 비용은 일부 누락될 수 있습니다. Singular branch nodes는 extension이 들어간 branch의 전체 비용이지 extension 때문에 추가된 순수 노드가 아닙니다. 여러 nested 호출의 노드는 중복될 수 있어 총합을 전체 노드에서 빼면 안 됩니다. Double extension은 구현하지 않았고 단일 extension의 경로 누적 예산을 기존처럼 제한합니다. RFP saved-nodes와 wrong-cut rate는 counterfactual oracle이 없으므로 측정하지 않았습니다.

RFP: 11 critical FEN에서 **17,893 opportunities / 65 cuts (약 0.36%)**, 참조 일치 개선 없음. 계속 experimental OFF입니다. IID/Singular/ProbCut은 얕은 corpus에서 발동이 거의 없었으므로 깊은 검사를 별도로 수행합니다. 이 결과만으로 기능을 제거하거나 margin을 느슨하게 하지 않았습니다. ProbCut의 기존 base/improving margin 상수는 유지했습니다. Depth 기반 새 margin 튜닝은 증거가 부족해 보류했습니다.

최종 guard 바이너리의 추가 deep fixture (`v05-deep`):

| 항목 | 관측 |
|---|---|
| Singular / 복잡한 위치 1M nodes | 4 tests, 2 extensions, excluded 11,253 nodes, extended branches 311,696 nodes, beta cutoff 0, PV update 1 |
| ProbCut / 위 위치 | 53 attempts, qsuccess 35, reduced success 35, cutoffs 35, 21,329 nodes |
| Verified null / quiet 위치 500k nodes | 2,901 attempts, 2,126 fail-high/cutoffs, 131 verification / 131 success / 0 rejection, verification 9,647 nodes, material rejection 30 |
| LMR / 위 quiet 위치 | 9,348 reductions, 37 re-search requests / 37 completed |
| IID / 시작 위치 depth 7 | 118,509 total nodes, IID calls 0 (TT 정보 존재) |
| Time / soft 70ms, hard 250ms | 기존/후보 모두 depth 5, 13,864 nodes, 135ms, soft stop 1회; factor는 1.0 대 1.5 |

IID의 실제 연결 여부는 별도 TT-OFF 진단(`v05-iid-missing-tt.json`)에서 **41 calls / 8,278 auxiliary nodes / seed 발견 41회**로 확인했습니다. TT-OFF 진단을 일반 TT-ON 효용 증거로 사용하지 않습니다. `v05-forensics.json`의 조회 위치는 Axiom/reference root move가 같아 suspect 후보 0개였습니다. 기록 cap 때문에 모든 감축을 조사한 것은 아니며 잘못된 감축이 없다는 증거도 아닙니다.

## 8. Corpus / Ablation / Interaction

`v05-critical`: 기존 실제 테스트 PGN 6게임에서 173개 위치를 Stockfish depth 12로 분석해 **100cp threshold 첫 crossing 11개**를 수집했습니다. 양 side를 따로 추적합니다. 랜덤 self-play FEN 생성물이 아닙니다. 메이트 score 4건을 제외하고, 음수 원시 차이 34건은 별도 기록 후 0으로 clamp했습니다. 음수 차이가 많다는 사실 자체가 reference depth/검색 노이즈의 한계입니다.

`reference_score_after`는 같은 root 관점에서 **실제 둔 수로 제한한 참조 검색 값**입니다. child-side score를 부호 변환 없이 빼지 않습니다. 원 PGN의 Axiom score/depth/PV를 안전하게 복원하지 않은 필드는 null이며 Stockfish 값을 대신 채우지 않았습니다. Category는 endgame 외에는 unclassified로 남겼습니다. 대규모/고심도 ground-truth corpus가 아니라 소규모 개발 회귀 자료입니다.

20,000 nodes / 11 FEN: All-On(RFP·신규 후보 OFF) 7/11. Correction OFF 또는 Continuation OFF는 6/11. Search Safety OFF는 평균 depth 4.00→4.27이지만 6/11. 단순히 depth만 보고 채택하지 않았습니다.

5 FEN depth 4: 기준 67,835 nodes, Correction OFF 64,353, Continuation OFF 67,390, Search Safety OFF 51,001; 이 표본의 참조 일치는 모두 4/5. 300ms fixed-time에서는 Search Safety OFF가 평균 depth 4.00→4.40, 일치 4/5로 동일했습니다. **기능을 끄면 효율이 나아지는 사례**이며, 11-FEN 결과와 상충하므로 제거를 확정하지 않았습니다.

Interaction: 요청한 6개 조합 각각 00/01/10/11, 5 FEN × 15,000 nodes (기준 포함 125회). Correction+RFP 조합에서는 Correction OFF가 4/5, ON이 2/5였고 RFP가 이를 바꾸지 못했습니다. 반면 더 큰 20,000-node 표본에서는 correction이 유리했습니다. 예산·표본에 민감한 효과이며 보편적인 승패 결론이 아닙니다. 전체 결과는 `v05-interactions/summary.json`.

## 9. CPU / Search efficiency / Profiling

| 분리 측정 | 0.4 | 0.5 |
|---|---:|---:|
| CPU: 5 FEN × 50,000 nodes, 총 시간 | 2684ms | 2504ms |
| CPU: 총 NPS | 93,145 | 99,840 |
| Search: 5 FEN depth 5, 총 nodes | 266,287 | 266,287 |
| Search: 위 실행 시간 | 3032ms | 3055ms |

위 표는 **시간 guard 수정 전 0.5 후보**의 측정입니다. 새 후보 OFF / proof OFF / 1 thread. 단일 순차 실행이라 CPU 차이를 확정적인 속도 향상으로 주장하지 않습니다. 고정 깊이에서는 node saving이 없고 시간도 조금 늘었습니다. 과거 0.3의 13.8배 수치를 0.5 결과로 재사용하지 않습니다.

별도 profile 실행의 inclusive 누계: legal generation 285.8ms, evaluation 284.2ms, ordering 155.1ms, correction lookup 28.4ms, TT probe 2.0ms. qsearch 4063.5ms는 재귀 inclusive라 벽시계보다 클 수 있습니다. TT context 생성·make/unmake·attack generation·history lookup은 개별 분리 타이밍을 아직 추가하지 않았습니다. 핵심 비용은 legal/eval/order에 있으며 측정 없이 TT context를 축약하거나 게임 규칙을 약화하는 최적화는 하지 않았습니다.

해당 profile의 TT hits/probes 2998/8161, pawn hits/(hits+misses) 60792/89571입니다. TT probe 분모는 recursive trusted probes만 포함합니다. Pawn cache 호출에는 평가 외 safety 호출도 포함됩니다.

## 10. 테스트 / 대국 / 최종 판단

첫 paired 실험은 완료했습니다. 20개 합법 opening, 색상 교대 100쌍, 2+0.02초, concurrency 2, 엔진별 Threads 1 / Hash 32 / proof OFF. 양쪽에 동일 Middlegame 9개 기능 + time_management, 후보에만 adaptive_time을 켰습니다. 비교는 0.4 대 시간 guard 전 0.5이며 timer 이외 계측/root 이력 비용도 포함하므로 adaptive-time 단독 인과 추정은 아닙니다.

**후보 73승 88패 39무 / 200게임, 46.25%**. Runner 추정 -26.11±43.52 Elo, SPRT LLR -0.29, 경계 ±2.94에 도달하지 않아 **inconclusive**. 더욱 중요한 실패는 time forfeit **후보 9회 / reference 6회**입니다. 불법 수는 0회였습니다. 결과 일부만 보고 시간패가 1회였다고 축소하지 않았습니다. 이 실험은 기력 개선이나 시간 안정성 통과가 아니며 **해당 후보를 기각/기본 OFF 유지**합니다.

데이터: `v05-paired-adaptive-time/{manifest.json,game-summary.json,games.pgn,console.log}`. 시간 guard 전 후보 SHA256 `F5388D867C5444241CF9ACF72260ED5E538DCECA6BAEFCCC323EB05F3CA48A83`; frozen binary도 폴더에 보존했습니다. 2+0.02는 매우 짧은 제한이며 일반적인 STC/LTC가 아닙니다. 시간 guard 수정 후 결과를 이 200게임과 합치지 않습니다.

최종 guard 빌드 검사/재대국 결과는 아래에 별도로 기록합니다.

최종 Release SHA256: `2759BE2D57FEA934ECB871A6AB87781D5DA72EA118C05BFB7ED9E8D8C72DA21F`.

Release/Debug 모두 CTest **3/3**, UCI smoke 통과. AddressSanitizer RelWithDebInfo도 **3/3 통과**했습니다. 최초 ASan 실행은 DLL 부재로 시작하지 못했으며, 설치된 공식 MSVC runtime DLL을 해당 빌드 폴더에 복사한 후 재실행했습니다. 메모리 오류를 무시하거나 검사를 제거한 것이 아닙니다. ASan 검사 시간 79.72초 (`v05-asan-runtimefix.log`). UBSan은 실행하지 않았습니다.

기존 1,569 checks 유지; audit에 auxiliary IID/excluded history·TT 격리 검사를 추가했습니다. 새 calibration 테스트는 correction bound/mask/mate exclusion, stack trend, root 이력, SAN/승격/castling, quiet mate 보존, clock reserve 및 제한 시간 검사를 포함합니다. 최종 0.5와 0.4의 **100 FEN × 3,000 nodes**에서 Middlegame bestmove/완료 depth/score type/score/nodes/PV가 모두 동일했습니다 (`v05-final-equivalence.json`). 새 후보가 기본 OFF인 설정의 동일성입니다.

최종 guard CPU 재측정: 250,000 nodes 고정에서 0.4 **2499ms / 100,040 NPS**, 0.5 **2480ms / 100,806 NPS**. 약 0.8% 차이로 노이즈와 구분하기 어렵습니다. 동일 depth 5는 양쪽 **266,287 nodes**, 시간 **2765→2764ms** (`v05-final-cpu.json`, `v05-final-efficiency.json`). 초기 후보의 더 큰 속도 차이를 최종 성능으로 내세우지 않습니다.

**시간 guard 후 재검사 완료:** 같은 외부 시간 제한/20개 opening/Threads/Hash/concurrency로 색상 교대 20쌍, 40게임 (`v05-guard-smoke`). 후보 **16승 19패 5무**, 시간패 **후보 0 / reference 1**, 불법 수 0. SPRT LLR -0.05로 inconclusive입니다. 이 40게임은 운영상 시간 안정성 smoke이며 Elo 상승이나 시간패 완전 제거의 증명이 아닙니다. 기존 200게임과 합산하지 않았습니다. `calibrated_lmr`, `adaptive_time`, `rfp`는 계속 기본 OFF입니다.

## 11. 재현

```powershell
.\build.ps1 -Configuration Release -NativeAVX2
.\build.ps1 -Configuration Debug -NativeAVX2
.\tools\sanitize.ps1
node tests\calibration-tools.mjs
.\build\Release\axiom-0.5-calibration.exe --analyze --middlegame --feature time_management --feature adaptive_time --time 1000 --soft-time 300 --depth 30 --proof-nodes 0
.\build\Release\axiom-0.5-calibration.exe --analyze --experimental --calibration-samples 256 --profile --nodes 50000 --time 0 --proof-nodes 0
node tools\calibrate.mjs --engine build\Release\axiom-0.5-calibration.exe --fens results\v05-critical\critical.jsonl --out results\NEW-ABLATION --nodes 20000 --matrix ablation
```

다른 matrix: `single`, `correction`, `candidates`, `interaction`. `--nodes 0 --depth 4`는 depth 비교, `--nodes 0 --time 300 --depth 30`은 시간 비교. 결과 폴더를 재사용하지 않습니다. Stockfish와 PGN converter 경로는 `critical-corpus.mjs --reference ... --converter ...`로 명시합니다.

## 12. 알려진 위험 / 제거 결정 / 다음 후보

새 후보 3개는 기본 OFF. 삭제한 기존 heuristic은 없습니다. 깊은 corpus와 충분한 대국 증거가 부족해 제거하지 않았으며, 낮은 발동 빈도가 곧 무가치하다는 뜻은 아닙니다. RFP 유지 비용과 효용은 계속 비교해야 합니다. 고정 시간 우위 없이 LMR 보호를 더 늘리는 것도 보류합니다.

다음 유망 후보는 **검증된 실제 대국 corpus 확장 + correction 성분의 동일 holdout 표본 평가 + Search Safety의 과보호 조건별 ablation**입니다. 기대 Elo 숫자는 제시하지 않습니다. 현재 11개 FEN의 분류/원 PGN telemetry, depth 18+ 참조 재평가, 장시간 LTC, 대규모 holdout 검증은 미완료입니다. 이 보고서의 계측 기반 구축과 실제 기력 향상 입증을 구분해야 합니다.

사용자 후속 0.6 제안은 별도 작업 범위입니다: 보호 이유별 Search Safety mask, frozen correction snapshot/동일 holdout target, 300~1000 critical FEN, depth 18~22 참조, lazy/staged MovePicker 비교. 이번 0.5에서 구현했다고 주장하지 않습니다. 원인별 수치화되지 않은 confidence를 임의 확률로 표시하지 않고, 관측 근거와 반사실적 A/B 결과를 먼저 제공해야 합니다.
