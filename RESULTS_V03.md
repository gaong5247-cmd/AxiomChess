# Axiom 0.3 CPU — 검증 결과

## 전달 파일

- 최신: `build/Release/axiom-0.3-cpu.exe` — C++20, MSVC x64 Release, AVX2.
- Visual Studio: `build/AxiomChess.slnx`, 시작 프로젝트 `axiom`.
- 설정/구현 설명: `CPU_MIDDLEGAME.md`.
- 기존 실행 중인 `axiom-0.3.exe`는 종료하거나 덮어쓰지 않았습니다. 최종 파일은 새 이름입니다.

```powershell
.\build\Release\axiom-0.3-cpu.exe --analyze --middlegame --proof-nodes 0 --depth 12 --time 3000
```

`--proof-nodes 0`은 CPU 시간을 일반 탐색에 쓰는 설정입니다. 독립 메이트 증명을 사용하려면 생략하세요. UCI GUI에서는 최신 exe를 등록한 뒤 `Middlegame=true`를 설정합니다. AVX2 미지원 PC에서는 `build.ps1`을 `-NativeAVX2` 없이 실행해 다시 빌드해야 합니다.

## 속도

같은 5개 FEN, 각각 30,000노드, 1스레드, proof 0, 추가 휴리스틱 off의 비교:

- 0.2: 합계 21,805ms.
- 최종 0.3 CPU: 합계 1,575ms.
- 약 **13.8배**. 5개 모두 수·점수·완료 깊이·노드 수가 일치했습니다.

검색 내부 시간 기준의 소규모 진단입니다. 일반적인 속도 또는 Elo 보장이 아닙니다. 실행 환경/포지션/옵션에 따라 달라집니다. 원시 결과와 바이너리 해시는 `results/cpu-final-v3-v2.json`에 있습니다.

## Stockfish 18 depth 18 비교

백/흑을 포함한 합성 미들게임 후보 1,500개 중 1,170개를 검사하고, 참조 기준 오차 포지션 500개 수집 목표에 도달해 정상 종료했습니다. `completion.json`은 `stopped:false`, `target_reached:true`입니다. 남은 330개까지 모두 검사한 것은 아닙니다.

Axiom: Middlegame, 5,000노드, 1스레드, proof 0. Stockfish: depth 18, MultiPV 3, 4스레드. Top-3 밖의 수는 원래 루트에서 해당 수로 제한해 같은 깊이까지 따로 평가했습니다.

| 지표 | 결과 |
|---|---:|
| 측정 포지션 | 1,170 |
| Top-1 agreement | 50.94% |
| Top-3 agreement | 75.04% |
| CP 비교 가능 포지션 | 1,086 |
| 평균 CP 손실 | 20.70 |
| 중앙값 CP 손실 | 0 |
| 95백분위 CP 손실 | 97 |
| 최대 CP 손실 | 599 |
| >200cp blunder | 14 / 1,086 = 1.29% |
| >100cp major error | 46 / 1,086 = 4.24% |
| 별도 메이트 비교 포지션 | 84 |
| 메이트 관련 실패 신호 | 16 |

메이트 점수를 가짜 CP로 환산하지 않았습니다. CP 통계와 메이트 실패를 함께 봐야 하며, CP blunder rate만으로 전체 실수율을 해석하면 안 됩니다. 유한 참조 탐색의 변동으로 선택 수 평가가 전체 검색의 최선 평가보다 높게 나온 사례도 110개 있습니다. 이들의 CP loss는 정의상 0으로 처리했으며 음수 원래 차이는 원시 결과에 보존했습니다.

파일:

- `results/regression-balanced-v3-depth18/reference-errors.fens`: 수집된 500개 참조 오차 FEN.
- `results/regression-balanced-v3-depth18/positions.jsonl`: 전체 비교의 FEN/수/PV/점수/가설 태그.
- `results/regression-balanced-v3-depth18/summary.json`: 요청한 지표.
- `results/regression-balanced-v3-depth18/manifest.json`: 조건과 SHA-256.

이는 static-guided/random legal play로 생성한 표본이며 실제 사용자 대국에서 모은 실패 사례가 아닙니다. 같은 생성 대국의 연관된 포지션과 큰 기물 불균형도 포함합니다. 이 비율은 실전 정확도나 독립 holdout 성능 추정치가 아닙니다. 오류 FEN도 수학적으로 최선이 증명된 데이터가 아니라 Stockfish 유한 탐색 기준입니다. 98% 정확도·실수율 0%·Elo 향상·SPRT 통과는 달성했다고 주장하지 않습니다.

## 최종 파일 검증

Release/Debug의 C++ 검사 1,569개, UCI 통합 검사, 회귀 통계/프로토콜 검사, 깊은 탐색 기능 활성화 검사가 통과했습니다. 최종 파일과 Stockfish 본 검사에 사용한 최적화 전 파일을 1,500개 FEN × 5,000노드/Middlegame 설정으로 비교한 결과, 수·점수·PV·완료 깊이·노드 수의 불일치는 **0개**였습니다. 따라서 위 참조 결과와 최종 파일의 연결을 이 고정 조건에서 확인했습니다. 모든 포지션/시간 제어에서의 동등성을 증명한 것은 아닙니다.

기록: `results/final-equivalence.json`. `node tests/validate-results.mjs`로 원시 자료에서 통계를 다시 계산하고, 500개의 고유 오류 FEN 및 최종 바이너리 해시를 검사했습니다.

최종 Release SHA-256: `C3F9EEE0E129B625A066737EAD0A5999CD03B38BF38955C9EC24E37720B38A09`.

## 남은 약점

큰 손실과 메이트 관련 실패가 아직 남아 있습니다. 제한된 5,000노드에서는 깊이 1~3에서 끝나는 포지션도 있어, 보호 신호를 추가하는 것만으로 해결되지 않습니다. 진단 일부에서는 같은 깊이의 비선택적 탐색보다 추가 깊이가 도움이 됐습니다. 다음 기력 변경은 이 실패 파일로 원인을 좁히되, 별도 대국/holdout으로 과적합과 퇴행을 확인해야 합니다.

이 버전은 CPU 최적화와 요청한 탐색/평가/회귀 검사 기반을 구현한 연구용 엔진입니다. 참조 오류 500개를 모두 수정한 버전은 아닙니다.
