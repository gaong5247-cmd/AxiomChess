# Axiom 0.3 — CPU / Middlegame

## 실행 설정

```powershell
.\build\Release\axiom-0.3-cpu.exe --analyze --middlegame --depth 12 --time 3000
```

UCI에서는 `setoption name Middlegame value true`를 사용합니다. 이 프리셋은 correction, continuation, capture_history, countermove, dynamic_lmr, strategic_eval, king_safety, search_safety, pawn_cache를 켭니다. Singular extension, verified null move, ProbCut, history/SEE pruning, IID 등은 구현되어 있지만 프리셋에는 포함하지 않았습니다. 필요하면 `--feature verified_null`처럼 따로 켜거나 `--experimental`을 사용합니다. 기존 기본 null move도 단순 엔드게임에서는 기물/반수 조건으로 비활성화됩니다.

기능이 구현되었다는 사실은 기력 향상의 증거가 아닙니다. 기본 휴리스틱을 강제로 승격하지 않고 프리셋과 개별 스위치로 비교할 수 있도록 했습니다. 보수적인 보호도 같은 노드 예산에서는 탐색 깊이를 낮출 수 있습니다.

## 추가 내용

- Correction History: 검색 오차를 5개 제한된 정수 테이블에 반영하며, 깊이에 따른 moving average로 보정합니다.
- Continuation History: 1/2/4/6 ply 문맥. Capture History와 countermove를 함께 정렬에 반영합니다.
- Dynamic LMR: 깊이/순번, history, PV/TT-PV, cut node, improving을 반영합니다. 줄인 깊이의 검색이 alpha를 넘으면 원래 깊이로 재검색합니다.
- Search Safety Layer: PV, TT, check, promotion, high history, singular extension, strategic move, improving, tactical-threat 신호가 있으면 late-move reduction과 history/SEE 가지치기를 하지 않습니다. 전략 후보는 통과폰 전진·폰 브레이크·아웃포스트·오픈 파일·캐슬링/단순 엔드게임 킹 이동을 포함합니다. 위협 탐지는 공격받는 퀸과 무방비 기물 등 제한된 휴리스틱이며 모든 전술 위협을 판별하는 해결기가 아닙니다.
- Quiescence에서도 체크/승격 외에 넓은 창, improving, high history, 공격받는 무방비 기물의 포획을 delta/SEE pruning에서 보호합니다. Stand-pat과 유한 q-depth 제한은 남아 있으므로 수학적 안전성을 보장하지 않습니다.
- 전략 평가: outpost, weak square, bad bishop, rook file, pawn break, space, coordination, trapped piece, passed-pawn potential.
- 킹 안전: king-ring 공격 칸, 공격 기물별 가중치/attack units, 다중 공격자, 킹 주변 파일과 pawn shelter, queen+rook 협력, 기물 phase 보간.
- Pawn cache: 4,096-entry, 두 색의 실제 pawn bitboard를 모두 비교해 해시 충돌을 확인합니다. 폰만으로 결정되는 정보만 저장하고, 킹 위치·기물 배치·phase에 의존하는 항은 별도 계산합니다. worker별 캐시이며 `pawn_cache_hits/misses`를 출력합니다.

## CPU 최적화와 측정

임시 합법성/체크/SEE 계산에서 반복 이력을 만들지 않고 실제 검색 수에서만 기록합니다. SEE는 전체 합법 수가 아닌 대상 칸의 합법 포획만 생성합니다. Zobrist 난수는 사전 계산하고, 반복 문맥 키는 FEN 포맷팅 대신 고정 길이 상태 바이트를 사용합니다. 평가에서는 문자열 맵 생성을 진단 시점으로 미루고 공격 맵과 폰 정보를 재사용합니다. 증명·반복 판정의 실제 상태 비교는 유지합니다.

MSVC Release `/O2`, 공격적 인라이닝, IPO/LTCG, 선택적 `/arch:AVX2`를 사용합니다. 수동 SIMD 커널이나 완전한 증분 평가를 구현했다는 뜻은 아닙니다. 기본 단일 스레드이며 Lazy SMP는 `--threads N`으로 켭니다. 스레드 수 증가가 항상 속도나 기력 향상을 보장하지는 않습니다.

`results/cpu-final-v3-v2.json`: Stockfish 참조 작업을 마친 후, 같은 5개 FEN에 각 30,000노드/깊이 상한 15/증명 예산 0/1스레드/새 휴리스틱 off로 비교했습니다. 두 엔진은 모든 포지션에서 수·점수·완료 깊이·노드 수가 같았습니다.

| 포지션 순서 | 0.2 시간(ms) | 0.3 시간(ms) |
|---|---:|---:|
| 1 | 3,905 | 328 |
| 2 | 10,255 | 421 |
| 3 | 1,671 | 208 |
| 4 | 4,727 | 263 |
| 5 | 1,247 | 355 |
| 합계 | 21,805 | 1,575 |

이 소규모 검사에서는 약 **13.8배** 빨랐습니다. 엔진이 보고한 검색 시간 기준이며, 일반적인 속도 보장이나 Elo 결과가 아닙니다. 미들게임 프리셋까지 켠 설정의 속도를 뜻하지도 않습니다. 바이너리 SHA-256은 결과 JSON에 기록했습니다. 초기 최적화본의 11.4배 측정은 `results/cpu-v3-v2-clean.json`에 별도로 보존했습니다. 추가로 quiescence에서 방문 대상이 아닌 quiet move를 정렬 전에 제외해 불필요한 작업을 줄였습니다.

## Stockfish 회귀 검사

```powershell
# 새 후보 표본 생성. 기존 파일은 덮어쓰지 않습니다.
.\build\Release\axiom_corpus.exe tests\another-corpus.fens 1500 20260913

# 기본 참조 깊이 18, Axiom 5,000노드, 첫 500개 참조 오차 포지션 수집 목표.
# --target 0이면 목표 오류 개수와 무관하게 --limit까지 전부 검사합니다.
node .\tools\stockfish-regression.mjs --fens tests\middlegame-balanced.fens --limit 1500 --target 500 --depth 18 --threads 4 --out results\new-regression
node .\tests\regression-metrics.mjs
node .\tests\regression-uci.mjs
.\tests\search-features.ps1
```

Node.js는 검사 프로세스 간 UCI 통신만 담당합니다. 엔진과 표본 생성기는 C++입니다. Axiom은 1스레드/Hash 32 MB/ProofNodes 0/Middlegame true, Stockfish는 Hash 64 MB/MultiPV 3을 사용합니다. `--threads`는 **참조 Stockfish** 스레드 수입니다. 다중 스레드 참조는 실행 간 완전히 결정적이지 않습니다.

Top-3에 없는 Axiom 수는 원래 루트에서 Stockfish `searchmoves`로 같은 깊이까지 강제 평가합니다. 양쪽 점수는 같은 루트 차례 기준입니다. CP loss는 `max(0, reference best - reference selected)`이며 원래 차이도 기록합니다. 음수 차이가 생길 수 있는 유한 검색의 불확실성을 숨기지 않습니다. Mate 점수는 가짜 CP로 변환하지 않고 CP 통계에서 제외하며 mate regression을 별도로 셉니다. 기존에 승리 메이트가 없어진 경우/새 패배 메이트가 생긴 경우만 세므로 메이트 거리 악화 전체를 포착하지는 않습니다.

출력: `manifest.json`(설정/바이너리 및 corpus SHA-256), `positions.jsonl`(FEN/PV/깊이/점수), `summary.json`, `reference-errors.fens`, `completion.json`. 결과 폴더는 덮어쓰지 않습니다. 실행 도중 해당 결과 폴더에 `STOP` 파일을 만들면 현재 포지션을 완료한 뒤 안전하게 종료합니다. `completion.json`의 stopped/target_reached를 반드시 확인하세요.

통계는 Top-1/Top-3 agreement, mean/median/p95/max CP loss, blunder >200cp, major error >100cp, mate regression을 포함합니다. CP 비율의 분모는 CP 비교가 가능한 포지션 수이고, agreement의 분모는 측정된 전체 포지션 수입니다. p95는 nearest-rank 방식입니다. 오류만 모은 FEN 파일로 계산한 비율을 일반 포지션의 정확도처럼 사용하면 안 됩니다.

분류 태그는 tactical/positional/king safety/pawn structure/endgame 등의 **가설**입니다. `--diagnose N`은 >100cp 사례 일부를 같은 깊이 비선택적 탐색 및 추가 깊이로 비교해 LMR/pruning 또는 horizon 후보를 표시하지만 인과관계를 확정하지 않습니다. 실제 변경의 승인은 별도 holdout corpus와 색 교환 대국/SPRT가 필요합니다. `tools/run-sprt.ps1`은 준비되어 있지만 실제 SPRT는 수행하지 않았습니다.

표본은 static-guided/random legal play로 만든 합성 포지션입니다. `middlegame-balanced.fens`에는 백 차례 791개, 흑 차례 709개가 있습니다. 실제 사용자 대국의 실패 FEN을 제공받아 수집한 것이 아니며 같은 대국에서 나온 상관된 포지션도 있습니다. 98% 정확도, 큰 실수 0%, Stockfish 수준의 기력을 주장하지 않습니다.

## 정확성 검사

Release/Debug에서 기본 C++ 검사 1,569개와 UCI 통합 검사를 실행했습니다. 기본 규칙/캐슬링/앙파상/승격/perft/증명 트리 재검증 외에, 무작위 합법 포지션에서 이전 평가와 새 빠른 평가의 값 일치, pawn cache 유무의 값 일치, 대상 칸 SEE와 전체 합법 재포획 oracle의 일치, 전체 반복 이력 보존을 확인합니다. 시작 포지션 perft 5는 4,865,609입니다.

별도 `search-features.ps1`은 100만 노드의 전술 포지션에서 correction/continuation/capture history, singular extension, ProbCut, Dynamic LMR, safety/cache의 실제 실행 횟수를 확인하고, 다른 50만 노드 포지션에서 verified null search가 수행되는지 검사합니다. 실행 횟수 검사는 옵션이 작동함을 보여 줄 뿐 그 옵션의 안전성이나 Elo 향상을 입증하지 않습니다.
