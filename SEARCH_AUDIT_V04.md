# Axiom Chess 0.4 — correctness / search audit

2026-09-12. C++20, Visual Studio 18 Community, Release x64 AVX2 + LTCG.
최신 실행 파일: `build/Release/axiom-0.4-audit.exe`.
기존 사용자 실행 파일은 덮어쓰지 않았습니다. 수정 전 바이너리와 소스는 `baseline/audit-v03.exe`, `baseline/audit-v03-source.zip`에 보존했습니다.

## 발견한 오류와 수정

1. **체크 상태 qsearch horizon:** qply 20에서 합법적인 체크 회피를 검색하지 않고 static eval을 반환했습니다. `7k/8/8/8/8/8/4r3/4KQ2 w - - 0 1`로 수정 전 실패를 재현했습니다. 이제 비체크에서만 qply 제한을 적용합니다. 절대 MaxPly에서 체크 중이면 현재 반복을 중단해 마지막 완료 반복으로 돌아갑니다. 임의 평가를 확정 점수처럼 반환하지 않습니다.
2. **검증 결과 불일치:** `--middlegame --depth 5 --time 0 --proof-nodes 0 --verify` 시작 위치에서 bestmove가 같으면 기존 depth 5 / score 38을 유지하면서 verification depth 4 / score 0을 보고했습니다. 이제 검증 성공 시 bestmove가 같아도 점수·깊이·후보 목록을 함께 교체합니다. 이 얕은 독립 검증은 게임 이론적 증명이 아닙니다.
3. **synthetic null subtree:** 반복/50수 가상 이력의 무효화와 함께 insufficient-material까지 건너뛰었습니다. 이력과 무관한 insufficient-material 판정은 유지합니다.

추가 안전성 수정: ProbCut의 improving 추가 margin까지 반환값에서 일관되게 차감하고, null verification 성공 시 null score와 검증 score 중 작은 값을 반환합니다. checking bestmove는 quiet correction 학습에서 제외합니다. 이들은 안전성/일관성 수정이며 모든 경우의 잘못된 수를 입증했다는 뜻은 아닙니다. qsearch 반환은 fail-soft best로 통일했습니다.

## 이미 있던 기능 / 이번 구현

- Correction/Continuation/Capture History, Countermove, Dynamic LMR, Singular Extension, Verified NMP, ProbCut, SEE/history pruning, 전략 평가, king safety, pawn cache는 기존 구현을 감사하고 재사용했습니다. 새로 만든 기능으로 중복 계산하지 않습니다.
- PVS null-window 실패 후 full-window 재검색 구조 및 TT mate normalization은 이미 올바르게 구현돼 있었습니다. 비교 스위치 `--no-pvs`, `--no-tt` 및 직접 경계값 테스트를 추가했습니다. 정확 비교에는 `--no-selective --no-tt`도 함께 사용합니다.
- 새 `--feature rfp` / UCI `Feature_rfp`: 얕은 non-PV, 비체크, 안정 평가, 유효 TT move 없음, 충분한 비폰 기물, 메이트 점수 영역 밖에서만 적용합니다. 캡처/체크/승격/공격받는 기물이 있으면 보호합니다. 실제 내부 탐색 호출에서 cutoff가 일어나는 테스트가 있으며 stub이 아닙니다. **기본 OFF**, Middlegame 프리셋에도 넣지 않았습니다.
- IID는 기존의 depth-2 보조 탐색으로 정렬 정보를 얻는 구현을 유지했습니다. 동일 목적의 IIR을 중복 추가하지 않았습니다. auxiliary 탐색은 선택적 가지치기·일반 TT 저장·history 학습에서 격리됩니다.
- Search Safety에 불안정한 eval 및 단 하나의 합법 수 조건을 보강했습니다. Null/ProbCut의 endgame 제한도 유지·보강했습니다. 모든 유일한 방어 수를 알아내는 전술 증명기는 아닙니다.
- worker별 256개 move-order scratch와 ordinal tie-break 정렬로 일상적인 정렬 할당을 제거했습니다. 비정상적으로 큰 수 목록은 안전한 vector fallback을 사용합니다. 루트 raw eval은 재사용하되 학습된 correction은 각 수에서 다시 반영합니다. Board make/unmake 구조를 유지했습니다.
- qnodes, TT/null cutoffs, PVS scouts/researches, 완료된 LMR 재검증, check evasions/horizon interruptions, RFP 컷을 JSON 계측 및 SMP 합산에 연결했습니다. 기존 history/correction/singular/ProbCut 계측도 유지합니다.

## 변경 파일

`include/axiom/engine.hpp`, `src/search.cpp`, `src/feedback.cpp`, `src/main.cpp`, `tests/audit.cpp`, `tests/audit-equivalence.mjs`, `CMakeLists.txt`, `build.ps1`, `tools/run-sprt.ps1`, `tools/bench.ps1`, `README.md`, 이 보고서.
추가 측정 산출물은 `results/audit-*`, 단계별 바이너리는 `baseline/audit-*`입니다. `tools/run-sprt.ps1`은 별도 ReferenceEngine 및 version 비교를 지원하며 두 바이너리를 결과 폴더에 동결합니다.

## 재현 명령

```powershell
.\build.ps1 -Configuration Release -NativeAVX2
.\build.ps1 -Configuration Debug -NativeAVX2
.\tests\search-features.ps1 -Engine .\build\Release\axiom-0.4-audit.exe
node .\tests\audit-equivalence.mjs
.\build\Release\axiom-0.4-audit.exe --analyze --middlegame --time 3000 --depth 20
.\build\Release\axiom-0.4-audit.exe --analyze --feature rfp --proof-nodes 0 --time 3000
```

측정 스크립트는 기존 결과를 덮어쓰지 않습니다. 재실행 시 새 출력 경로를 지정하거나 기존 결과를 별도로 보존해야 합니다.

## 검증 결과

### 측정 결과: before / after

5개 FEN, depth 5, 상한 200,000 nodes, 1 thread, proof OFF, 기본 기능, 시간 제한 없음. 3회 반복 합계 (`results/audit-final-depth5-{1,2,3}.json`):

| 지표 | 0.3 수정 전 | 0.4 최종 |
|---|---:|---:|
| 총 탐색 시간 | 11,501 ms | 10,857 ms |
| 총 노드 | 865,620 | 798,861 |
| 총 노드 / 총 시간 NPS | 75,265 | 73,580 |

시간 약 5.6% 감소, 노드 약 7.7% 감소, NPS 약 2.2% 감소입니다. 노드 수가 달라졌으므로 순수 CPU throughput 개선으로 해석하면 안 됩니다. 전 위치 depth/score는 동일했고, 시작 위치는 같은 score 37의 bestmove가 b1c3에서 e2e3으로 바뀌었습니다. **Elo 측정이 아닙니다.** 공유 데스크톱에서 측정했으며 시스템 노이즈를 완전히 통제하지 않았습니다.

단계별 결과: correctness 수정은 `audit-correctness-stage.json`, RFP 연결은 `audit-rfp-stage.json`, allocation 최적화는 `audit-allocation-stage.json`에 보존했습니다. RFP 단계에서는 컷 0회이고 같은 노드/수/점수였습니다. allocation 단계의 5-FEN 시간은 3,558→3,535 ms, 동일 노드 266,287개로, 시간 차이는 작아 노이즈와 구별할 수 없습니다. 메모리 할당 제거의 구조적 효과와 실측 속도 이득을 구분합니다.

Middlegame 동일 프리셋, 1초/FEN의 고정 시간 비교 (`audit-final-fixedtime.json`):

| bench FEN 순서 | 0.3 depth / move | 0.4 depth / move |
|---|---|---|
| 1 시작 위치 | 6 / d2d4 | 6 / d2d4 |
| 2 복잡한 미들게임 | 4 / e2a6 | 4 / e2a6 |
| 3 룩 엔드게임 | 7 / b4c4 | 6 / b4f4 |
| 4 전술 위치 | 5 / c4c5 | 5 / c4c5 |
| 5 메이트 위치 | 3 / f5g6 | 3 / f5g6 |

3번의 완료 깊이 감소와 수 변경은 추후 참조 평가/반복 측정이 필요한 회귀 후보입니다. 상대 엔진의 점수와 새 엔진의 점수 차이를 cp loss로 계산하지 않았습니다. 이번 0.4에 대해 500-FEN Stockfish 전체 재측정은 수행하지 않았습니다.

### 실제 대국 파일럿

`results/audit-version-pilot/`: Fastchess 1.8.2-alpha, 2+0.05초, 1 thread, 같은 9개 Middlegame 기능, proof OFF, 두 개 smoke opening, 색상 교대 10쌍, 총 20게임. **0.4 기준 8승 9패 3무, 47.5%**. SPRT LLR -0.02, 경계 ±2.94에 도달하지 않아 **결론 없음**. runner 추정치 -17.39 ±149.13 Elo는 표본이 너무 작고 오프닝이 협소해 기력 결론에 사용할 수 없습니다.

20게임 모두 PGN termination `normal`; 불법 수·크래시·시간패는 관측하지 않았습니다. 한 번 `PV continues after threefold repetition` 경고가 있었습니다. 합법 PV 검사와 별개로, 엔진은 청구 가능한 무승부를 선택 옵션으로 모델링하지만 runner는 3회 반복을 즉시 종료로 취급합니다. 대국은 정상 무승부로 종료됐으며 GUI/runner 친화적인 PV 종료 처리는 아직 별도 개선사항입니다.

바이너리 및 오프닝 hash와 전체 인수는 manifest, 수순은 games.pgn, 계측은 console.log/runner.log에 보존했습니다. 사용한 [Fastchess 공식 릴리스](https://github.com/Disservin/fastchess/releases/tag/v1.8.2-alpha)의 배포 ZIP SHA256은 `007ce550dc809510b34622a1aeb43fd443d40284045bef5c1bc39c8941114143`으로 검증했으며 동봉 라이선스를 유지했습니다.

최종 0.4 Release SHA256: `BB5AA0C936F9203F4F441D15BD4D41A11340B9F8671FE09A904C27D8B19B0B43`.

### 회귀 검사 범위

기존 1,569개 체크에 더해 별도 `axiom_audit` 타깃을 추가했습니다. 기존 perft, mate/stalemate/draw, make/unmake/hash 검사는 유지합니다. audit는 qsearch horizon, TT EXACT/LOWER/UPPER와 mate ply 정규화, 검증 score/depth, 12개 PVS 대 full-window 비교, 합법 PV, mate 1/2/3 및 quiet mate, rook underpromotion, poisoned capture, x-ray/pin/promotion SEE, discovered check, checking intermezzo, 유일 합법 수, 반복/50수/메이트 우선권, 폰 엔드게임 null 차단을 검사합니다.

LMR fixture는 fail-high 재검색이 실제 발생하고, 완료된 full-depth 재검증 횟수가 요청 횟수와 일치해야 통과합니다. RFP는 내부 non-PV quiet node에서 실제 컷 1회 및 체크/폰 엔드게임 컷 0회를 확인합니다. `search-features.ps1`은 실제 깊은 탐색에서 correction, continuation, capture, singular, ProbCut, verified null, safety, pawn-cache 경로가 활성화되는지 확인합니다.

**최종 결과:** Release/Debug 각각 CTest 2/2 타깃 및 UCI smoke 통과. Release deep feature activation 통과. Allocation 최적화 전후 100 FEN × 3,000 nodes에서 bestmove/완료 depth/score type/score/nodes/PV가 모두 동일했습니다 (`audit-equivalence.json`). 이 동일성 검사는 CPU 최적화 단계 사이의 비교이며 correctness 수정 전 0.3과 결과가 모두 같다는 뜻은 아닙니다. Debug build/검사 출력은 `audit-debug-build.log`에 남겼습니다. ASan/UBSan 검사는 이번 MSVC 빌드에서 수행하지 않았습니다.

## 남은 위험 / 다음 우선순위

검색 안전성은 게임 이론적 정확성을 뜻하지 않습니다. 장기 check sequence는 절대 ply 제한에서 반복이 중단될 수 있고, SEE는 국소 교환 모델입니다. zugzwang 테스트는 null 차단을 확인하지만 모든 엔드게임 WDL을 검증하지 않습니다. Fathom/Syzygy는 선택 연동이며 이번에 모든 엔드게임을 tablebase로 검증하지 않았습니다. SMP 비결정성, 메모리 할당이 남은 legal move 목록 및 full-history TT context의 비용도 남아 있습니다.

다음 기력 개선의 우선 후보는 **실제 대국에서 수집한 diverse critical FEN으로 correction/history 가중치와 LMR 보호 임계값을 조정한 뒤, 고정 시간 paired SPRT로 확인하는 작업**입니다. 장기적으로 heuristic TT의 context 비용 최적화도 유망하지만 repetition/50수 및 증명 레이어의 정확성을 보존하는 별도 설계가 필요합니다. 예상 방향일 뿐 Elo 수치는 아직 예측하거나 보장하지 않습니다.

새 RFP는 얕은 5-FEN bench에서 cutoff 0회였으므로 속도나 기력 이득을 주장하지 않습니다. 직접 활성 경로 테스트와 실전 빈도/효과 측정은 다른 문제입니다. 기존 `RESULTS_V03.md`의 500 오류 FEN/Stockfish 지표 및 13.8배 속도 수치는 **0.3의 과거 결과**이며 0.4 측정으로 재사용하지 않습니다.
