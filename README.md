# Axiom Chess — C++20

## AxiomChess 2.0

`axiom-2` 브랜치는 CPU 전용 classical UCI 엔진을 `v2/`에 독립 구현합니다.
기존 엔진은 비교와 검증을 위해 보존했습니다. NNUE, 학습 모델, 오프닝 북 없이 동작합니다.

- [Windows x64 실행 파일](dist/windows-x64/axiom-2.exe) · [실행 안내 및 SHA256](dist/windows-x64/README.md)
- [빌드 및 UCI 사용법](AXIOM_2_BUILD.md)
- [구현 범위, 테스트, 실측 결과와 한계](AXIOM_2_REPORT.md)
- [기존 엔진 감사](docs/AXIOM_2_AUDIT.md) · [검증 원본](results/axiom2)

동일 노드 기력 개선은 아직 입증되지 않았으며 DecisionImpact와 RefutationSearch는 기본 OFF입니다.
아래는 기존 버전의 개발 기록입니다.

**Phase 2 현재 후보:** `Feature_legal_fast_path` (기본 OFF). 체크가 아니며
핀 없는 비킹 기물의 일반 수에서 legality용 push/pop을 생략합니다. 기존
생성기는 oracle로 보존합니다. 구현 범위·실측·미완료 작업은
[RESULTS_V07_PHASE2.md](RESULTS_V07_PHASE2.md)를 확인하세요. Phase 1의
`reuse_move_facts`와 합쳐 켠 성능을 검증한 결과는 아직 없습니다.

Visual Studio 18 Community용 독립형 체스 엔진입니다. Python 및 학습 모델 없이 동작합니다. 합법 수 생성, 평가, 탐색, 강제 메이트 증명을 직접 구현했습니다.

**현재 0.7 비용 절감 연구 단계:** `build\Release\axiom-0.7-cost.exe`. QSearch·move ordering·결과 직렬화 책임을 분리하고, 연구 빌드 전용 hot-path profiler와 중복 check/SEE 재사용 후보를 추가했습니다. 후보 `Feature_reuse_move_facts`는 기본 OFF입니다. 구현 범위, 반복 측정, 검증과 미완료 항목은 [RESULTS_V07.md](RESULTS_V07.md)에 기록합니다. 아래 버전 설명은 과거 기록입니다.

**0.6 연구용 구현:** `build\Release\axiom-0.6-attribution.exe`. Safety reason mask, 강제 root replay, frozen correction holdout, qsearch 계측과 실험용 partial ordering/time guard를 추가했습니다. 전체 요청의 미완료 항목과 실측은 [RESULTS_V06.md](RESULTS_V06.md), 기본값과 승격 조건은 [FEATURE_REGISTRY.md](FEATURE_REGISTRY.md)를 확인하세요. 기력 향상이나 98% 정확도를 보장하는 릴리스가 아닙니다.

**최신 0.5 Search Calibration:** `build\Release\axiom-0.5-calibration.exe`. Correction 성분별 측정, LMR forensic 기록, root/time 후보, PGN 기반 critical corpus, ablation/interaction 도구를 추가했습니다. 후보 기능은 기본 OFF이며 기력 상승이 입증된 프리셋으로 간주하지 않습니다. 최신 결과·한계·실행법은 [RESULTS_V05.md](RESULTS_V05.md)를 참고하세요. 아래 0.3/0.4 설명은 버전별 과거 기록입니다.

**최신 0.4 correctness audit:** 체크 상태의 horizon 처리, 검증 결과의 score/depth 일관성, synthetic null subtree의 무승부 처리를 수정했습니다. 보수적 RFP(기본 OFF), 추가 탐색 계측 및 고정 크기 정렬 scratch를 추가했습니다. 실행 파일은 `build\Release\axiom-0.4-audit.exe`이며, 아래 0.3 예제에서도 실행 파일명만 바꿔 사용할 수 있습니다. 변경점·검증·한계는 [SEARCH_AUDIT_V04.md](SEARCH_AUDIT_V04.md)를 참고하세요. 기존 0.3 결과는 과거 측정값으로 보존합니다.

**0.3 CPU / 미들게임:** 기존 search feedback에 전략 평가, king-ring/attack-unit 기반 킹 안전, Search Safety Layer, 정확한 pawn cache를 추가했습니다. 보드 임시 이동·SEE·평가의 중복 작업을 줄였습니다. 새 기능은 `--middlegame`으로 켭니다. 아직 Elo 상승을 입증한 설정은 아니며, 기본값에서는 pawn cache만 켜져 있고 Threads는 1입니다. [CPU_MIDDLEGAME.md](CPU_MIDDLEGAME.md)에 설정·속도 측정·회귀 검사 방법을 정리했습니다. 기존 탐색 기능 상세는 [SEARCH_FEEDBACK.md](SEARCH_FEEDBACK.md)를 참고하세요.

## 실행

완료된 테스트와 Stockfish 참조 지표: [RESULTS_V03.md](RESULTS_V03.md). 참조 기준 오류 FEN 500개를 수집했습니다. 기력/98% 정확도/SPRT 통과를 주장하지 않습니다.

빌드된 최신 엔진: `build\Release\axiom-0.7-cost.exe`. 아래는 0.3에서 도입된 기능의 사용 예시입니다(실행 파일명만 바꾸면 됩니다).

PowerShell에서 이 폴더로 이동한 뒤:

```powershell
# 새 미들게임 설정으로 JSON 분석. 시간은 밀리초, 0은 시간 제한 없음.
.\build\Release\axiom-0.3-cpu.exe --analyze --middlegame --depth 12 --time 3000

# 새 휴리스틱 전체 + 공유 TT 3스레드 (실험 설정)
.\build\Release\axiom-0.3-cpu.exe --analyze --experimental --threads 3 --depth 8 --time 3000

# 기능 하나만 비교할 때
.\build\Release\axiom-0.3-cpu.exe --analyze --feature correction --proof-nodes 0 --nodes 20000 --time 0

# 모든 후보 수를 선택적 가지치기 없이 재검증 (검증 깊이 최대 4).
.\build\Release\axiom-0.3-cpu.exe --analyze --depth 3 --time 0 --verify

# 2수 메이트: 거리는 반수(ply) 단위이므로 3이 출력됩니다.
.\build\Release\axiom-0.3-cpu.exe --prove --fen "7k/8/8/5K2/5Q2/8/8/8 w - - 0 1" --plies 3 --nodes 100000 --time 0

# 합법 수 생성 검사: 197281
.\build\Release\axiom-0.3-cpu.exe --perft 4
```

인수 없이 실행하면 UCI 엔진으로 동작합니다. 체스 GUI의 엔진 등록에서 `axiom-0.3-cpu.exe`를 선택하거나, 콘솔에 다음을 입력합니다.

```text
uci
isready
setoption name Middlegame value true
position startpos moves e2e4 e7e5
go depth 5
```

`bestmove`를 확인한 뒤 다음 명령을 입력합니다. 탐색 중 `isready`, `stop`에 응답하며 `quit`으로 종료합니다. `go movetime 2000`, `go nodes 100000`, `go infinite`, `go verify depth 3`, `verify depth 3`를 지원합니다. `go mate N`은 제한 내에서 메이트 증명을 시도하며, 발견하지 못하면 미증명 탐색 결과를 반환할 수 있습니다. `ponder`와 `searchmoves`는 지원하지 않습니다.

`setoption name ProofNodes value 0`으로 별도 메이트 증명을 끌 수 있습니다. 기본값은 15,000 노드, 5 ply이며 시간 제한의 최대 약 20%를 먼저 사용합니다. 모든 예산은 안전한 중단 지점에서 검사하므로 수 생성·정렬·증명 재검증 중의 소폭 시간 초과는 가능합니다.

새 UCI 옵션: `Middlegame`(보수적인 미들게임 프리셋), `Experimental`(모든 실험 기능), `Feature_correction` 등 개별 `Feature_*` check 옵션, `Threads`(1..16), `Selective`(기본 true; false는 선택적 가지치기/LMR 비활성화). 일반 탐색에서는 별도의 전술 패턴 라벨을 생성하지 않습니다. `--diagnostics` 또는 `--verify`로 요청하세요. 상세 이벤트 횟수는 JSON의 `search_stats`에 출력됩니다.

## Visual Studio에서 열기 / 빌드

`build\AxiomChess.slnx`를 열고 `Release | x64`, 시작 프로젝트 `axiom`을 선택합니다. 프로젝트 폴더 자체를 Visual Studio의 **폴더 열기**로 열어 CMake를 사용해도 됩니다.

```powershell
.\build.ps1
.\build.ps1 -NativeAVX2
.\build.ps1 -Configuration Debug
```

스크립트는 다음 설치를 사용하며 빌드 후 C++ 검사와 UCI 통합 검사를 실행합니다.

```text
C:\Program Files\Microsoft Visual Studio\18\Community
```

CMake 4.x의 `Visual Studio 18 2026` 생성기와 C++ 데스크톱 개발 도구, Windows SDK가 필요합니다. 기본 엔진에 외부 라이브러리는 없습니다. 스크립트는 상속된 `Path`/`PATH` 중복을 현재 프로세스에서만 정리합니다.

제공한 Release 실행 파일은 이 CPU의 AVX2 지원을 확인하고 `-NativeAVX2`로 빌드했습니다. AVX2 미지원 PC에서는 이 옵션 없이 다시 빌드하세요. Node.js는 선택적인 Stockfish 회귀 검사 도구에만 필요하며, 엔진 실행에는 필요 없습니다.

## 결과를 읽는 방법

| 분류 | 의미 |
|---|---|
| `EXACT_TABLEBASE` | Syzygy 조회가 성공하고 이 엔진의 보수적인 이력 조건을 만족함 |
| `PROVEN_MATE` | 실제 체크메이트 또는 모든 상대 방어를 포함한 메이트 증명 트리를 재검증함 |
| `PROVEN_FORCED_RESULT` | 스테일메이트·자동 무승부 등 판정 가능한 종료 상태 |
| `SEARCH_RESULT` | 완료된 유한 깊이 탐색. 게임 전체의 승패 증명이 아님 |
| `HEURISTIC_EVALUATION` | 완료된 반복 탐색이 없는 경우의 정적 평가 |

점수는 루트 차례의 플레이어 기준 센티폰입니다. `mate_distance`는 반수 단위이며, 증명된 경로에서 가장 긴 상대 방어 길이입니다. 반복 깊이 증가 증명으로 최초 성공 깊이를 찾습니다. 이 값은 DTZ와 다릅니다.

각 후보 수는 점수, 깊이, 증명 상태, 메이트 거리, PV, 노드 수, 전술 후보 라벨, 테이블베이스 결과, 경계 유형을 제공합니다. `UPPER` 후보가 같은 점수로 여럿 표시될 수 있습니다. null-window에서 얻은 상한이지 모두 동일하게 좋은 수라는 뜻은 아닙니다. `EXACT_SEARCH`도 해당 유한·선택적 탐색 모델 내의 결과만 뜻합니다. 강제 메이트가 증명되면 다른 후보들은 정적 평가 상태로 남을 수 있습니다.

`--verify`는 모든 루트 후보를 독립적인 전체 창으로 최대 깊이 4까지 재탐색하고, null-move·LMR·SEE/delta 가지치기를 끕니다. 후보별 `tactical_verification`에 검증 점수와 가장 강한 탐색 응수를 기록합니다. 다른 수가 나오면 그 검증 결과를 선택하며 깊이도 갱신합니다. 제한 시간이 먼저 끝나면 `completed:false`입니다. 이 모드 역시 게임 전체의 최선성 증명은 아닙니다.

선택적 탐색에서 큰 메이트 점수를 얻었더라도 별도 증명에 성공하기 전에는 `SEARCH_RESULT`로 유지합니다. 증명된 패배라는 이유만으로 미증명 후보보다 높은 우선순위로 선택하는 방식은 사용하지 않습니다.

## 구현 내용

- 0x88 보드, 체크 합법성, 캐슬링, 앙파상, 4종 승격, FEN/UCI.
- Zobrist 해시와 가역 이력. 반복 판정은 해시 이후 실제 상태 문자열까지 비교합니다. 실제 가능한 앙파상만 반복 동일성에 반영하고 원래 EP 상태는 FEN에 유지합니다.
- 3회 반복/50수 청구를 선택 가능한 0점 행동으로 취급. 5회 반복/75수 자동 무승부 및 일반적인 기물 부족 판정. 체크메이트를 먼저 판정합니다. 청구가 최선이면 JSON에 `claim_draw:true`를 표시하고 UCI GUI에 알립니다. 실제 청구는 GUI가 처리해야 합니다.
- 독립적인 AND/OR 메이트 증명 및 증명 트리 재검증. 공격자는 한 수, 방어자는 모든 합법 수를 포함합니다. 예산 초과는 `UNKNOWN`이며 무승부로 단정하지 않습니다.
- Iterative deepening, alpha-beta/negamax, PVS, aspiration windows, TT, killer/history ordering.
- 체크·잡기·승격 우선 정렬, 법적으로 가능한 같은 칸 재포획을 계산하는 SEE.
- 체크 중 stand-pat을 사용하지 않는 quiescence. 잡기·승격·초기 checking moves 및 체크 회피를 탐색합니다. 종료 보장을 위해 q-depth 20, 추가 quiet check 3단계, 전체 ply 120으로 제한합니다.
- null-move는 비루트·비체크·충분한 기물 조건에서만 사용하며 위험한 단순 엔드게임에서는 비활성화합니다. 가상 null 경로의 반복 판정과 TT 저장을 차단합니다.
- LMR은 늦은 quiet non-PV 수에 적용하며 alpha를 넘으면 원래 깊이로 다시 탐색합니다. 체크·잡기·승격·TT·killer 수는 제외합니다.
- 기물, 활동성, 이동성, 킹 안전, 폰 구조, 공간, 위협, 통과폰, 엔드게임 항을 공개하는 phase 보간 평가.
- 포크·핀·스큐어·발견 체크·노출 퀸·갇힌 기물·체킹 중간수·승격 경쟁 후보 라벨. 이 라벨은 패턴 탐지이며 해당 전술의 성공을 증명하지 않습니다. 중간수와 조용한 방어의 실제 효과는 일반 합법 수 탐색에서 다룹니다.
- 별도 finite-graph retrograde 모듈. terminal → predecessor 전파 및 거리 계산, 완전한 그래프에서의 미해결 사이클 무승부 판정.

## Syzygy (선택)

기본 배포 실행 파일에는 Fathom이나 테이블 데이터가 포함되어 있지 않습니다. Fathom 소스 체크아웃과 Syzygy WDL/DTZ 파일을 직접 준비하면 다음처럼 연결할 수 있습니다.

```powershell
cmake --preset vs2026 -DFATHOM_DIR=C:/deps/Fathom
cmake --build --preset release
.\build\Release\axiom-0.3-cpu.exe --analyze --fen "7k/8/8/5K2/5Q2/8/8/8 w - - 0 1" --syzygy C:/tablebases
```

UCI에서는 `setoption name SyzygyPath value C:\tablebases`를 사용합니다. Fathom의 root WDL/DTZ API를 호출하고 반환된 수의 합법성을 재확인합니다. WDL은 -2..2이며 cursed win/blessed loss는 50수 규칙상 무승부로 취급합니다. halfmove clock이 0이 아닌 경우 이력에 대한 완전성을 주장하지 않고 참고 데이터로만 노출합니다. 데이터 부재/조회 실패는 정상 탐색으로 이어집니다. DTZ 권장 수는 WDL 보존용이며 최단 메이트 수를 뜻하지 않습니다.

Fathom 연결 코드는 기본 빌드에서 비활성화되어 있으며, 이 환경에는 테이블 파일이 없어 실제 Syzygy 조회는 검증하지 않았습니다. 참고 API: [Fathom tbprobe.h](https://github.com/jdart1/Fathom/blob/master/src/tbprobe.h).

## 범위와 확장

이 버전은 실행 가능한 연구용 엔진입니다. Stockfish 수준의 기력, Elo, 일반 체스의 완전 해결을 주장하지 않습니다. null-move·LMR·SEE 가지치기는 실용적 가정에 의존하므로 수학적인 승패 증명에 사용하지 않습니다. 조용한 최선수는 일반 탐색과 메이트 증명 모두에 포함됩니다.

자체 retrograde는 완전한 유한 그래프를 입력받는 해결 코어입니다. 전체 체스 엔드게임 상태/역방향 합법 수 열거기와 테이블 저장 포맷은 아직 구현하지 않았습니다. 외부 테이블 없이 일반 포지션의 강제 무승부·강제 승리를 모두 증명하지 않습니다. 전술적인 기물 획득만으로 게임 승리를 증명했다고 표시하지 않습니다. 드문 모든 dead-position 형태를 판정하는 일반 해결기도 없습니다.

`include/axiom/chess.hpp`가 규칙/상태, `engine.hpp`가 분석/증명 API, `retrograde.hpp`가 별도 역방향 해결 API입니다. `src/main.cpp`가 UCI 및 CLI 진입점입니다. `tests/tests.cpp`는 표준 perft, 특수 규칙, 증명 변조, 방어 누락, 무승부 청구, 검색 결과와 중단을 검사합니다.
