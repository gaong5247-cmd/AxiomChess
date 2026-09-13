# Search feedback extension — 0.2

이 문서는 0.2에서 도입한 기능의 설계 기록입니다. 최신 실행 파일은 `build/Release/axiom-0.3-cpu.exe`이며, 추가된 CPU 최적화·미들게임 프리셋·폰 캐시는 [CPU_MIDDLEGAME.md](CPU_MIDDLEGAME.md)를 참고하세요. 아래 0.2 명령은 보존된 이전 바이너리를 대상으로 합니다.

구현 철학: **Solve first · Verify dangerous pruning · Reuse search feedback · Measure every change.**

이 확장은 수작업 평가 함수와 알파베타 탐색을 유지합니다. 히스토리 값은 검색 과정에서 갱신하는 제한된 정수 테이블이며 ML 모델·자기대국 훈련 데이터·신경망은 없습니다. 승패의 정확한 증명과 선택적 탐색 결과의 구분도 유지합니다.

## 활성화

모든 실험 기능은 기본 `false`입니다. 구현 및 기능 검증만으로 Elo 상승을 가정해 기본값으로 승격하지 않았습니다.

```powershell
# 모든 기능, 3개 검색 스레드. 메이트 증명은 먼저 실행합니다.
.\build\Release\axiom-0.2.exe --analyze --experimental --threads 3 --depth 8 --time 3000

# 단일 기능, 고정 총 노드 예산. 옵션은 반복 가능합니다.
.\build\Release\axiom-0.2.exe --analyze --feature correction --feature continuation --proof-nodes 0 --nodes 60000 --time 0 --depth 8

# 전체 설정에서 특정 기능 제외
.\build\Release\axiom-0.2.exe --analyze --experimental --disable-feature probcut --time 2000
```

UCI:

```text
setoption name Feature_correction value true
setoption name Feature_continuation value true
setoption name Threads value 3
position startpos
go wtime 60000 btime 60000 winc 500 binc 500
```

`Experimental=true`는 아래 14개 기능을 켭니다. SMP의 스레드 수는 별도 `Threads` 옵션입니다. `ucinewgame`은 TT·히스토리를 초기화하며, 기능 설정이나 선택적 탐색 정책이 바뀌면 이전 경계를 버립니다. 동일 정책에서는 대국 내 검색 피드백과 TT를 재사용합니다. 검증 탐색에 사용한 TT는 다음 일반 탐색에 섞이지 않습니다.

## 추가된 설계 항목 19–36

| 번호 | 항목 / 옵션 | 이 구현의 정책 |
|---|---|---|
| 19 | `correction` | pawn structure, material counts, minor configuration, non-pawn configuration, previous-move context의 5개 테이블. side-to-move별 분리. 보정은 ±256cp 이내이며 raw 평가를 함께 유지 |
| 20 | `continuation` | 1·2·4·6 ply 전의 이동 기물/도착 칸과 현재 기물/도착 칸의 조합. 정렬과 LMR에 합산 |
| 21 | `capture_history` | 공격 기물·도착 칸·피획 기물별 히스토리. 앙파상의 피해 기물은 pawn. SEE와 함께 정렬 |
| 22 | `countermove` | 직전 이동 기물/도착 칸에 대응하는 cutoff 응수 저장 및 우선 정렬 |
| 23 | `singular` | 깊이 6 이상, 충분히 깊은 non-upper TT 수에 대해 그 수를 제외한 reduced search. 다른 수가 임계값보다 낮을 때 1 ply 연장, 경로당 총 2회까지 |
| 24 | `probcut` | non-PV·비체크·깊이 5 이상. SEE가 나쁘지 않은 capture를 raised-beta qsearch 후 reduced search로 확인 |
| 25 | `verified_null` | static eval 조건을 추가한 NMP. 깊이 6 이상 null fail-high를 원래 포지션에서 NMP 없이 재검증하고 성공한 경우에만 cutoff |
| 26 | improving feature | 현재 보정 평가와 같은 쪽 2 ply 전 평가 비교. 체크/미설정 값 제외. Dynamic LMR, history pruning, ProbCut margin에 반영 |
| 27 | `dynamic_lmr` | 깊이/수 번호 기반 R에 cut-node, history, improving, PV, TT-PV를 반영. reduced search가 alpha를 넘으면 반드시 원래 깊이 재탐색 |
| 28 | `history_pruning` | 낮은 깊이의 late quiet 수 중 심하게 나쁜 history를 가진 수 제외. PV·체크·승격·TT·killer·singular 연장 수 보호 |
| 29 | `see_pruning` | 낮은 깊이에서 깊이별 손실 한계보다 나쁜 capture/quiet 수 제외. 첫 탐색 수 및 위의 보호 대상 제외 |
| 30 | `mate_distance` | 터미널 검사 후 ply로 가능한 메이트 점수 창을 제한. 결과를 별도 증명으로 승격하지 않음 |
| 31 | `iid` | TT 수가 없고 깊이 5 이상일 때 얕은 내부 탐색의 PV로 정렬 수 확보. 보조 탐색의 TT·피드백 갱신 차단 |
| 32 | TT PV flag | 이전 PV 포함 여부를 저장. Dynamic LMR 및 교체 점수에 사용 |
| 33 | `tt_policy` | 4-entry bucket에서 depth + PV bonus + exact bonus − age penalty로 교체. 깊은 exact entry 보호, 오래된 정보의 갱신 허용 |
| 34 | `Threads` | Lazy SMP. 공유 TT와 worker별 히스토리/보드/stack. 정렬 tie-break·aspiration 폭·목표 깊이를 달리함 |
| 35 | `time_management` | bestmove 변경 횟수와 연속 안정도를 통해 soft budget 조절. hard deadline은 초과해서 늘리지 않음 |
| 36 | `--diagnostics`, `--verify` | 전술 패턴 탐지기는 루트 진단에서만 사용. 일반 노드에서는 SEE·정렬·qsearch·합법 수 탐색으로 전술 처리 |

보정 업데이트는 비체크·비청구·비메이트의 quiet bestmove가 있는 실제 탐색 노드에 제한합니다. upper/lower bound의 방향이 보정 오차와 일치할 때만 업데이트합니다. 보정은 깊이에 따른 bounded moving average를 사용하여 관측 오차에 수렴합니다. 이동 히스토리는 bounded gravity update를 사용하며 앞서 실패한 후보에 음의 보상도 줍니다. null, excluded-move, 내부 보조 탐색은 일반 TT 경계나 보정 업데이트를 쓰지 않습니다.

Singular/ProbCut/NMP 검증은 유한 탐색에 대한 실용적 확인입니다. 상대의 모든 향후 수에 대한 승패 증명을 뜻하지 않습니다. 확정 메이트는 기존 독립 AND/OR solver와 certificate replay를 통과해야 합니다.

TT는 정확한 반복 문맥 문자열을 유지하므로 compact lock-free TT는 아닙니다. bucket 접근을 mutex로 보호하고 엔트리는 복사해서 사용합니다. `Hash`는 문맥 문자열을 포함한 대략적인 메모리 목표이며 엄격한 전체 메모리 상한은 아닙니다. 아직 SIMD·bitboard·증분 평가에 대한 성능 튜닝은 하지 않았습니다.

## SMP / 시간 / 중단

메인 검색이 메이트 증명/테이블 조회를 먼저 수행하고 필요하면 보조 스레드를 시작합니다. 보조 스레드는 증명 작업과 root diagnostic을 중복하지 않습니다. 최종 수는 메인 스레드의 마지막 완료된 반복에서 선택하며 helper가 만든 TT 정보를 사용할 수 있습니다.

노드 예산은 모든 스레드와 메이트 증명에서 공유합니다. `stop`, `quit`, 새 `position`, 설정 변경 시 실행 중인 스레드를 회수합니다. 미완료 반복의 수를 최종 결과로 내보내지 않습니다. 별도 `go verify`는 helper를 종료한 뒤 휴리스틱을 끄고 수행합니다.

UCI의 남은 시간 기반 검색에는 soft/hard budget을 구분합니다. bestmove가 3회 이상 연속 안정적이면 soft budget의 70%, 매번 바뀌면 150%를 목표로 하되 hard deadline으로 제한합니다. 명시적인 `movetime`은 hard limit으로 유지합니다. CLI 테스트에는 `--feature time_management --soft-time 300 --time 1000`을 사용할 수 있습니다. 스케줄링·수 생성·정렬 작업으로 인한 작은 시간 초과는 가능합니다.

## 측정 및 기본값 승격

1. 기존 0.1 실행 파일은 `baseline/axiom-0.1.exe`와 SHA-256 기록으로 보존했습니다.
2. 기능별 비교는 같은 0.2 바이너리의 옵션 하나만 바꿔 공통 인프라 변경을 분리합니다. 0.2의 all-off 설정 자체가 0.1과 비트 단위로 같은 엔진은 아닙니다.
3. 고정 노드 benchmark는 합법성·깊이·이벤트 횟수·시간 진단입니다. 노드 수 감소나 깊이 증가는 Elo 증거가 아닙니다.
4. 기본값 승격에는 독립적인 opening corpus, 색 교환 paired games, 적절한 시간 제어의 실제 SPRT 결과가 필요합니다. 짧은 테스트 통과 후에는 긴 시간 제어도 별도로 확인합니다.
5. 10,000국 상한에 도달해도 LLR 경계를 넘지 못했다면 미결정입니다. 기능 실패 시 스위치를 끄며 자동 소스 삭제/merge는 하지 않습니다.

진단 실행:

```powershell
.\tools\bench.ps1 -Features correction -Nodes 10000 -Output .\results\correction-bench.json
.\tools\bench.ps1 -Features all -Threads 3 -Nodes 10000 -Output .\results\smp-bench.json
.\tools\bench.ps1 -Features all -Reference .\baseline\axiom-0.1.exe -Output .\results\original-comparison.json
```

실제 SPRT는 별도 C++ 대국 실행기 **Fastchess**를 사용합니다. 스크립트는 다운로드·설치하지 않습니다. 설치한 Fastchess와 충분한 EPD opening corpus를 지정합니다.

```powershell
# H0=0, H1=+5 logistic Elo, alpha=beta=0.05, 색을 바꾼 최대 5,000쌍.
.\tools\run-sprt.ps1 -Fastchess C:\tools\fastchess.exe -Openings C:\books\openings.epd -Feature correction

# correction을 승인한 이후 continuation만 추가하는 다음 실험.
.\tools\run-sprt.ps1 -Fastchess C:\tools\fastchess.exe -Openings C:\books\openings.epd -BaseFeatures correction -Feature continuation

# SMP는 reference와 candidate CPU 사용량이 다름을 별도로 해석해야 합니다.
.\tools\run-sprt.ps1 -Fastchess C:\tools\fastchess.exe -Openings C:\books\openings.epd -Feature lazy_smp -Threads 1 -CandidateThreads 2
```

스크립트는 실행 바이너리 사본, binary/opening SHA-256, 옵션, 난수 seed, 게임 상한, PGN, console log와 runner log를 새 결과 폴더에 저장합니다. 기본 concurrency는 1입니다. 양쪽의 proof budget은 0으로 맞춥니다. 프로세스의 exit code 0만으로 SPRT 통과라 표시하지 않습니다. `-DryRun`은 경로/인수 계획만 출력하며 대국을 하지 않습니다.

`tests/smoke-openings.epd`의 두 포지션은 명령 구성 smoke test용입니다. 이 파일을 반복해서 사용한 10,000국은 충분히 다양한 독립 표본을 대신하지 못합니다.

## 이 환경의 검증 기록

- 원래 규칙/perft/증명 테스트 유지.
- 양·음 history 포화, correction 방향/상한, 4가지 continuation 거리, capture/countermove 키와 초기화 검사.
- TT 깊은 exact/PV 보호, 문맥 분리, aging, 다중 스레드 snapshot 검사. aging 점수의 unsigned underflow를 회귀 검사에서 발견하여 수정.
- 실험 기능의 실제 업데이트, 3-thread 공유 노드 상한, 합법 bestmove, verification 후 증명 분류 유지 검사.
- 일반 UCI handshake 및 실험 SMP 상태의 `isready`/`stop`/`quit` 검사.
- `results/experimental-bench.json`, `results/experimental-bench-v2.json`: 개발 중 바이너리의 진단 결과이며 각각의 SHA-256을 함께 기록했습니다. 최종 0.2와 0.3의 속도 비교는 `results/cpu-v3-v2-clean.json`에 있습니다.
- 실제 Elo/SPRT 대국: **미실행**. Fastchess가 현재 실행 경로에 없으며 `run-sprt.ps1`은 dry-run까지 확인했습니다. 어떠한 옵션도 Elo-gaining으로 승인하지 않았습니다.

구현 참고 맥락: [Stockfish 18 공식 소개](https://stockfishchess.org/blog/2026/stockfish-18/)는 Correction History를 search 개선으로 설명합니다. 이 프로젝트의 수치·자료구조·정책은 자체 실험 구현이며 Stockfish와 같은 기력을 주장하지 않습니다. 대국 실행 인수는 [Fastchess 공식 매뉴얼](https://github.com/Disservin/fastchess/blob/master/man.md)을 기준으로 구성했습니다.
