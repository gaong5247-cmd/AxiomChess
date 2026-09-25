# AxiomChess 2.0 구현 및 검증 보고서

작성: 2026-09-26. 기준 소스: `49e12cac96dcc9d6e3a5e65ef1c9a0c818ab17fb`.
개발 브랜치: `axiom-2`. 기존 main 및 1.0 소스는 보존했다.

## 결과와 해석

기존 엔진의 플래그만 늘린 것이 아니라, `v2/`에 독립적인 bitboard 엔진과
UCI 실행 파일을 구현했다. Windows MSVC Release x64 빌드 및 실제 검색,
perft, oracle fuzz, UCI, SMP, Debug, ASan 검증을 수행했다.

고정 bench에서 계산 처리량은 개선되었다. 그러나 **동일 노드 기력 개선은
입증되지 않았다**. 동일 시간 소규모 대국은 유리했지만, 시간패와 실행 지연이
포함되어 있다. 이 결과를 확정 Elo나 장시간 production 인증으로 취급하지 않는다.
SPRT는 실행하지 않았다. NNUE, 신경망, 머신러닝, 학습 파일, 자가학습,
외부 추론 API, 엔진용 opening book은 사용하지 않았다. 실험 대국으로 평가
가중치를 학습하거나 튜닝하지 않았다.

최종 실행 파일: `build-v2/Release/axiom-2.exe`.
배포용 사본: `dist/windows-x64/axiom-2.exe`.
재현 방법: [AXIOM_2_BUILD.md](AXIOM_2_BUILD.md).

## 감사 및 구조 결정

항목별 KEEP/REWRITE/REMOVE/OPTIONAL 분류와 근거는
[전체 감사표](docs/AXIOM_2_AUDIT.md)에 있다.

버린 구조는 0x88 검색 코어, 매 수의 512바이트 보드 복사, 전체 보드 해시
재계산, 검색 노드와 TT에서의 반복 이력 문자열, 재귀적 legal-capture SEE,
노드별 vector 수 목록, 연구·증명·검색 정책이 섞인 기존 search 구조다.
기존 `src/`와 `include/axiom/`는 삭제하지 않고 1.0 비교 엔진 및 규칙 oracle로
유지했다. 기존 테스트, perft 포지션, CMake 기반도 유지했다. CMake에는 예외
취소 시 scope guard가 정상 실행되도록 `/EHsc`를 명시했다.

| 레이어 | 구현 파일 및 역할 |
|---|---|
| Position Core / Incremental State | `position.*`, `types.hpp`: 64칸 mailbox, piece/side occupancy, compact undo, 증분 hash/material/PST/phase/권리/EP/rule50 |
| Attack / Move Generation | `attacks.*`, `movegen.cpp`: bitboard 공격, checkers, pins, evasion masks, king/EP occupancy legality |
| Evaluation / Tactical Exchange | `evaluation.*`, `see.cpp`: tapered classical eval와 threshold SEE |
| Move Ordering / Search Memory | `ordering.cpp`, `history.hpp`: 고정 수 목록, killer/main/continuation/capture/counter/refutation |
| Search | `search.cpp`: PVS와 selective search |
| Tactical Search | `qsearch.cpp`: capture/promotion/check evasion 및 제한된 quiet check |
| Root Decision Scheduling | `root.cpp`, `stats.cpp`: 반복 심화, aspiration, MultiPV, 후보 기록과 실험 probe |
| Transposition Table | `tt.*`: 64바이트 clustered TT 및 동기화 |
| Time / Thread Manager | `time.*`, `threads.cpp`: 시간·노드 한도, ponder, Lazy SMP |
| UCI / Benchmark / Tests | `uci.cpp`, `bench.*`, `core_tests.cpp`, `search_tests.cpp`, `tools/v2-*.mjs` |
| Tablebase | `tablebase.*`: 선택적 Fathom WDL/DTZ adapter |

검색 hot path에는 FEN/string/map/unordered_map 또는 수 목록의 heap 재할당이
없다. 루트 반복 심화와 UCI 출력에는 vector/string을 사용한다. 공유 TT와
worker history는 검색 시작 전에 할당한다.

## Position, movegen, 규칙

Sliding attack은 portable ray traversal이다. Knight/king/pawn, between/line은
미리 계산한다. 일반 수는 pin/check mask로 합법성을 결정하고, king과 EP는
변경 후 occupancy로 확인한다. 모든 후보에 make/check/unmake를 수행하지 않는다.
castling 통과 칸, double check, promotion, pinned EP를 처리한다.

해시는 piece/state XOR로 증분 갱신한다. EP는 실제 합법적인 캡처가 있을 때만
repetition key에 포함한다. reversible history는 고정 크기 배열이며 null move
이하에서는 실제 대국의 repetition/claim을 사용하지 않는다. TT에는 rule50 및
이력 컨텍스트를 섞은 key를 사용한다. 정확한 문자열 이력 대조 대신 64비트
해시를 사용하므로 일반적인 확률적 해시 충돌 가능성은 있다.

3회 반복과 50수 청구는 현재 상태 및 의도한 다음 수를 고려한다. 5회 반복,
75수, 기본 insufficient material을 처리하고 메이트를 우선한다. claim은 선택지로
취급하며 실제 판정은 GUI의 책임이다. GUI PV는 claimable 상태에서 잘라 표시한다.
비정상 입력은 기존 position을 손상시키지 않는다. UCI의 reversible history
상한 및 고정 move list의 overflow는 명시적으로 검사한다.

## 검색 및 평가

- Iterative deepening, negamax alpha-beta/PVS, TT, mate distance pruning,
  제한된 check extension, 안정도·점수 변화 기반 aspiration을 구현했다.
  연속 aspiration 실패 4회 후에는 full window로 복귀한다.
- LMR table은 depth/move index의 로그값으로 미리 계산한다. PV/cut/improving/
  history/continuation/불안정도에 따라 조정하며 TT·killer·refutation·check·
  promotion 등을 보호한다. reduced fail-high는 full-depth로 복구한다.
  모든 제안 신호(예: 정교한 king-danger delta)를 개별 모델로 구현한 것은 아니다.
- Null move는 non-pawn material, check, rule50, promotion race 등을 제한한다.
  깊은 탐색 및 적은 기물 상황의 fail-high에는 verification을 수행한다.
  Pawn-only zugzwang에서는 null move를 사용하지 않는다.
- Singular extension은 TT 후보를 제외한 탐색으로 검사한다. 제외 탐색의 재귀적
  singular 호출을 막고 총 extension budget을 제한한다. Double/negative extension은
  구현하지 않았다.
- Reverse futility, futility, SEE/delta는 기본 ON이다. Razoring, history pruning,
  late quiet move-count pruning, deterministic ProbCut은 독립 스위치가 있고
  기본 OFF다. 모든 기법은 발동 횟수를 기록하며 추가 probe는 노드 비용도 기록한다.
- Ordering은 TT, 좋은 capture/승격, killer/counter/refutation, history quiet,
  나쁜 capture의 점수 구간을 사용한다. Capture promotion에는 승격 보너스도 준다.
- SEE는 local bitboard 교환을 사용하며 pinned/illegal king recapture를 검사한다.
  Threshold API와 첫 gain 조기 종료를 제공한다. 모든 교환 순서를 완전 탐색하는
  증명 알고리즘은 아니다.
- QSearch는 check 중 stand pat을 금지한다. Captures/promotions/evasions와 첫
  q-layer의 quiet checks를 검색한다. SEE/delta pruning 및 q-depth 제한이 있다.
  최대 ply에서 체크를 안전하게 해소할 수 없으면 해당 반복을 중단한다.

평가는 material/PST/phase를 증분 유지하고 tapered MG/EG로 결합한다. Mobility,
pawn structure, passed pawns, king ring/shield, space, threats, bishop pair,
rook files, outposts, 기본 endgame scaling을 추가했다. 모든 계수는 명시적인
classical 상수이며 학습된 계수가 아니다. 평가 정밀도는 여전히 개선 대상이다.

## TT, Decision-Impact, Refutation

TT entry는 16바이트이고 4개 entry가 64바이트 cluster를 이룬다. Full key,
move, score, static eval, depth, bound, PV flag, 5-bit generation을 저장한다.
Power-of-two indexing, depth/age/quality replacement, mate score normalization을
사용한다. Stripe mutex로 공유 읽기/쓰기를 보호한다. Lock-free TT는 아니다.
별도의 worker-local refutation cache를 둔다.

루트 후보는 score/depth/bound, 이전 점수, 안정도, PV churn, bound 실패 수,
uncertainty, risk, work를 기록한다. Score gap, instability, uncertainty, 누적
cost로 priority를 근사해 추가 탐색 대상을 정한다. 이는 실제 conspiracy number나
확률 모델이 아니다. 중요한 불안정 PV 내부에서는 alpha 근처 경쟁 후보에 추가
1 ply를 주는 제한된 실험 경로도 있다. 일반 PVS 기준선과 스위치로 비교 가능하다.

Refutation probe는 선택된 후보 이후 opponent reply를 별도 보조 탐색한다.
일반 selective shortcut을 제한하고 조용한 반박도 고려한다. 발견한 응수는
continuation history와 별개의 cache에 저장한다. 비용이 크며, 실험 대국에서
기본 PVS보다 불리했다. **DecisionImpact와 RefutationSearch는 기본 OFF**다.
기존 certificate 기반 mate solver는 1.0에만 남겨 두었으며 2.0 검색 결과에
검증되지 않은 PROVEN 라벨을 붙이지 않는다.

## UCI, 시간 관리, SMP, Syzygy

Hash, Threads, MultiPV, Ponder, Move Overhead, SyzygyPath와 표준 UCI 명령을
지원한다. `searchmoves`, node/depth/time limits, increments/movestogo, infinite,
ponderhit를 처리한다. 음수 clock은 0으로 제한해 즉시 응답할 수 있게 했다.
상태 변경은 이전 worker join 이후 수행하며 출력은 직렬화한다.

Soft/hard deadline과 bestmove/score/competition 안정도를 사용한다. 시간 검사는
256노드마다, stop 검사는 매 노드마다 한다. 작은 시간패가 관측되어 최종
Move Overhead 기본값을 20ms에서 **50ms**로 높였다. 이는 OS 또는 실행 환경의
긴 정지를 보장해 해결하는 기능은 아니다.

Lazy SMP는 TT와 전체 노드 예산을 공유하고 stack/history/killer/refutation은
worker-local로 둔다. 노드 제한은 atomic reservation으로 지킨다. 통계 문자열은
main worker 기준이고 UCI nodes는 모든 worker 합계다. MultiPV는 root exclusion
pass를 사용하며 일반 1PV는 추가 pass가 없다.

Fathom adapter는 root WDL/DTZ와 search WDL을 제공하되, 자동 cutoff/root 선택은
fresh zeroing 상태로 제한한다. 실제 Syzygy 파일을 이 환경에서 제공받지 않아
Fathom-enabled build 및 테이블 probe는 검증하지 않았다. 배포 바이너리는
Syzygy 미포함이며 설정 시 명확한 unavailable 메시지를 출력한다.

## 실제 검증

환경: Windows x64, Intel Core Ultra 5 125H, 논리 CPU 18개,
Visual Studio 18 / MSVC 19.51.36257.0, CMake 4.4.3. AVX2 옵션 OFF.

| 검사 | 실제 결과 |
|---|---|
| 기존 및 신규 Release CTest | 6/6 PASS |
| 신규 Debug CTest | 2/2 PASS |
| MSVC AddressSanitizer core/search | 2/2 PASS |
| Oracle 무작위 합법 수 대조 | 15,726 positions, seed 20260925, PASS |
| Make/unmake | 각 경로와 전체 되돌림에서 board/hash/material/PST/phase 일치 |
| 추가 회귀 | SEE threshold/EP/king recapture, intended repetition/rule50, mate/stalemate, null boundary, TT mate normalization |
| 검색 | PV legality, deterministic cold search, 모든 feature ON, exact node cap, SMP stop, concurrent TT record coherence |
| UCI | depth 10, movetime 1000, nodes 100000, searchmoves, MultiPV, ponder/hit, infinite/stop, 4T 반복, malformed FEN, 음수 clock, claim PV |

| Perft | 깊이 | 노드 |
|---|---:|---:|
| Startpos | 4 | 197,281 |
| Startpos | 5 | 4,865,609 |
| Kiwipete | 3 | 97,862 |
| Kiwipete | 4 | 4,085,603 |
| EP rook endgame | 4 | 43,238 |
| Promotion/castling fixture | 3 | 9,467 |
| Promotion/check fixture | 3 | 62,379 |

전체 로그는 `results/axiom2/`에 있다. 초기 구현 C–G는 통합 빌드/검증으로
진행했으며, 각 phase를 독립적인 릴리스로 검증했다고 주장하지 않는다.
Phase A 감사 → B 코어/oracle/perft → C–G 검색/UCI 통합 → H 실제 비교 및
발견 사항 보완 순서로 수행했다.

## Benchmark 및 대국

동일한 고정 5 FEN, Threads=1, Hash=32, depth cap 20,
각 포지션 100,000노드의 bench 결과:

| 엔진/실행 | 총 노드 | ms | NPS | checksum |
|---|---:|---:|---:|---|
| Axiom 1.0 | 500,000 | 4,458 | 112,157 | 7c0abdebcc2b5419 |
| Axiom 2.0 | 500,000 | 920 | 543,478 | 7cbd365beb923588 |
| Axiom 2.0 반복 | 500,000 | 905 | 552,486 | 7cbd365beb923588 |
| 최종 GUI 보완 후 재검사 | 500,000 | 1,458 | 342,935 | 7cbd365beb923588 |

920ms 실행은 기존 대비 약 4.85배 처리량이다. 1,458ms 재검사는 Debug 빌드와
함께 실행되었으며 처리 시간의 변동을 보여준다. 검색 트리의 노드 의미가 완전히
같지는 않으므로 NPS를 Elo로 환산하지 않는다. `equal-nodes.json`에는 별도
5-position corpus의 10k/50k/100k/500k/1M 비교 50개가 있다. Mate를 찾은 경우
2.0은 예산 전에 멈추므로 모든 행이 정확히 같은 노드를 소모하지는 않는다.

Fastchess 1.8.2, 고정 수동 오프닝 12개, paired colors, concurrency 2,
Hash 32MB, 최대 160수, 명시된 resign/draw adjudication으로 실행했다.
원본 command/config/log/PGN은 모두 보존했다. 다음은 최종 시간 여유 보완 전
측정이며 W/D/L은 표의 앞쪽 엔진 기준이다.

| 비교 | 조건 | W/D/L | Score | fastchess Elo 추정 / LOS | 시간패 |
|---|---|---|---:|---|---:|
| 2.0 vs 1.0 | 10k nodes, 24판 | 9/3/12 | 43.75% | -43.66 ±138.08 / 25.44% | 0 |
| 2.0 vs 1.0 | 10+0.1, 24판 | 18/1/5 | 77.08% | +210.72 ±192.07 / 99.94% | 2 |
| 2.0 vs 1.0 재검사 | 10+0.1, 뒤쪽 6 openings, 12판 | 9/2/1 | 83.33% | +279.59, interval NaN / 99.99% | 1 |
| 기본 PVS vs Decision+Refutation | 10+0.1, 12판 | 10/0/2 | 83.33% | +279.59, interval NaN / 99.90% | 2 |
| 1T vs 4T | 10+0.1, 12판 | 1/4/7 | 25.00% | -190.85 ±204.66 / 0.36% | 0 |

NaN은 fastchess가 이 작은 샘플에서 유효한 interval을 계산하지 못했다는 뜻이다.
SPRT는 미실행이며 이 추정을 인간 FIDE Elo나 확정된 엔진 Elo로 해석하지 않는다.
재검사 및 ablation은 독립적인 대규모 표본이 아니다.

첫 시간 대국에서 두 엔진이 거의 동시에 42,743/47,448ms 초과로 시간패했다.
이후 ablation에서는 runner heartbeat가 **131,354ms scheduling gap**을 기록한
시점에 양쪽이 121,887/122,409ms 초과했다. 공통 실행 지연의 증거는 있으나,
첫 지연까지 동일한 원인이라고 확정하지 않는다. 재검사의 25ms 시간패는 별도다.
문제 포지션의 500ms 제한 재현 20회는 441–493ms 범위에서 모두 응답했다.
시간패 판을 임의로 제거한 성적은 제시하지 않는다.

### 최종 실행 파일 실전 점검

Move Overhead 기본값 50ms와 반복 무승부 이후 GUI PV 제한을 반영한 최종 실행 파일로
10+0.1, 4개 opening의 색상 교대 **8판**을 추가 실행했다. Axiom2 기준 **7승 1무 0패**,
시간패 0회였으며 runner scheduling gap 기록도 없었다. 원본은
`results/axiom2/final-smoke/`에 있다. 이는 최종 바이너리의 소규모 회귀 점검이며,
확정적인 기력 상승이나 모든 시간 관리 문제의 해결을 입증하지 않는다.

## 한계와 다음 개선

- 동일 노드 기력 개선은 아직 입증되지 않았다. 더 큰 독립 opening corpus와 여러
  time control, SPRT, 장시간 soak 검증이 필요하다. 60+0.6/180+2 대회는 미실행이다.
- 실험 decision/refutation 정책은 비용 대비 성과가 부족하다. 루트 선택 규칙과
  probe budget을 검토해야 하며 현재 기본 OFF다. 내부 추가 탐색은 매우 제한적이다.
- 4T 소규모 결과는 유리하지만 모든 CPU/스레드 수에서의 scaling을 입증하지 않는다.
- TT의 이력 컨텍스트는 정확성에 보수적인 대신 transposition 재사용을 줄인다.
  Stripe mutex 경합과 atomic node counter 비용도 더 측정할 대상이다.
- Sliding attack에 magic/PEXT 테이블, 전용 AVX2 kernel은 없다. Portable 경로가
  기본이고 optional AVX2는 compiler code generation만 허용한다.
- Chess960, 새 DFPN solver, Fathom 실테이블 검증, ThreadSanitizer, Linux 빌드는
  완료하지 않았다. MSVC ASan은 data-race 검증을 대체하지 않는다.
- Insufficient material은 일반적인 기본 형태를 처리하며 모든 FIDE dead position을
  완전 판정하지 않는다. 고급 endgame scaling과 classical 평가 정확도는 개선 여지가 있다.
- Native GUI 제품을 직접 조작한 테스트는 하지 않았다. 실제 UCI 프로세스와
  fastchess로 호환성을 검증했다. 긴 호스트 정지 및 모든 실전 시간패가 해결되었다고
  주장하지 않는다.

실행 파일에는 학습 모델이나 book이 필요 없다. Windows x64의 표준 MSVC runtime은
필요할 수 있다. 결과를 재검증할 수 있도록 원본 측정값과 명령을 함께 제공한다.
