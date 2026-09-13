# Axiom 0.6 — Error Attribution 연구용 구현

2026-09-13. **전체 요청 완료나 Elo 개선을 주장하지 않는다.** 탐색 기본 정책은
0.5를 유지하고, 관측과 재검색 기반의 연구 경로를 구현했다. 아래 미완료 항목을
포함하므로 production strength release가 아니다.

## 변경 사항과 수정한 오류

- Safety 14개 이유를 bitmask로 분리. `legacy`가 0.5 보호 조건을 재현한다.
  `all`, `none`, `no-king`, `no-onlymove`, `no-strategic`, `no-trend`, 숫자 mask 지원.
  UCI `SafetyMask` 제공. RFP/singular 자체의 독립 guard는 아직 mask 대상이 아니다.
- `--force-root-move`는 합법성 검사, 시간 초과 fallback, root 검색 범위를 함께 제한한다.
  unrestricted proof/tablebase 결과로 강제 수가 바뀌는 경로를 차단했고,
  강제 root의 점수를 정상 root exact TT entry로 저장하지 않는다.
- `--no-lmr`, `--no-null`은 baseline 동작까지 실제로 끈다. 기존 feature OFF만으로
  해당 heuristic 전체가 꺼지는 것으로 오해하던 replay 문제를 방지한다.
- Frozen correction을 읽을 때 학습하지 않는다. 5개 전체 table snapshot과 FEN별
  raw/component 값을 저장하고, 같은 고정 reference target에 mask별 예측을 비교한다.
- qsearch 생성/검색/캡처/프로모션/체크/SEE·delta prune/cutoff 카운터 및 qply histogram.
- `--partial-ordering`: top-eight partial sort 실험. 완성된 staged lazy MovePicker가 아니다.
- `--time-guard`: 다음 iteration 예상 비용에 따른 보수적 시작 억제. UCI `TimeGuard`.
  queue/setup/overshoot/추정 오차를 기록한다. 기본 OFF이며 기존 adaptive_time도 OFF.
- Corpus 추출기의 잘못된 first/largest/all 동일 출력 생성을 제거했다. 이전 생성물은
  삭제하지 않고 COVERAGE.md로 사용 제한을 명시했다.

## 정확성 검증

Safety 분리 중간 단계: Release/Debug CTest 3/3, UCI smoke 통과.
100 FEN × 3,000 nodes에서 0.5와 bestmove/depth/nodes/score/PV 동일.
최종 코드도 Release 3/3 (5.61초), Debug 3/3 (39.53초), ASan 3/3 (78.66초),
UCI smoke와 `tests/v06-output.mjs`를 통과했다. 최종 binary의 100 FEN fixed-node
동등성도 통과했다 (`results/v06-final-equivalence.json`).
단순 합법 수/동등성 테스트는 전술 완전성 또는 Elo 검증이 아니다.

동결한 0.5 binary SHA256:
`2759BE2D57FEA934ECB871A6AB87781D5DA72EA118C05BFB7ED9E8D8C72DA21F`.
0.6 `baseline/v06-final.exe` SHA256:
`6744B4A4851DBCF583F99BEDAB3CAE99C9363F41E5C7B24F6FF83EEB150B61E7`.
`baseline/v06-final-source.zip` SHA256:
`D8DF18F7663BE41D0FE96331A2614D3056EE9F09345EE23187C9E3E1B3FDB108`.
0.5 source/binary snapshot은 덮어쓰지 않았다.

## Corpus와 correction holdout

실제 보존 대국 260개를 입력으로 3,056개 screening, 622개 confirmation 후
critical 300개 unique FEN 확보. 각 수의 손실은 Stockfish depth 18의 unrestricted와
forced-played score 차이로 측정. 270/300은 마지막 depth 16/17/18의 best move가
같고 score variance가 400 이하이다. 독립 depth 18/20/22 안정성 검사가 아니다.
모두 middlegame이고 과거 Axiom score/depth/PV는 미상이다.

Frozen holdout 300개, 별도 training roots 최대 100개 × 5,000 nodes. Holdout의
FEN identity와 같은 training root는 제외한다. 검색 자손까지 disjoint라고 보장하지
않는다. root previous token=0이므로 prevmove의 실전 가치를 평가한 실험은 아니다.

| Mask | MAE cp | RMSE cp | Median absolute error | P95 |
| --- | ---: | ---: | ---: | ---: |
| 0 raw | 93.717 | 131.748 | 69 | 290 |
| 31 all | 93.597 | 131.593 | 69 | 290 |
| 30 no pawn | 93.593 | 131.591 | 69 | 290 |
| 29 no material | 93.600 | 131.633 | 69 | 290 |
| 27 no minor | 93.543 | 131.540 | 69 | 290 |
| 23 no nonpawn | 93.603 | 131.601 | 69 | 290 |
| 15 no prevmove | 93.540 | 131.534 | 69 | 290 |

매우 작은 개선이며 component 승격/제거의 근거로 부족하다. 마스크가 남은 table의
평균을 재계산하므로 additive contribution을 뜻하지 않는다. all sign agreement 63%.
하나의 frozen state / critical-only 표본이므로 일반 포지션 성능으로 일반화하지 않는다.

## 1차 Safety / ordering / attribution 결과

30개 critical FEN × 20,000 nodes: legacy와 all reference agreement 2/30,
no-improving 4/30, no-pv와 no-tactical 3/30. 모든 이유가 유용/무용하다는 판정은
불가능하다. 보호 조건의 중첩 및 작은 search budget을 고려해야 한다.
일부 1차 Safety timing은 Debug 빌드와 겹쳤으므로 CPU 비교에 사용하지 않는다.

Partial ordering은 고정 depth 4에서 396,542 → 400,521 nodes, agreement 4/30 → 5/30.
고정 20k nodes 평균 depth 3.933 → 3.833. 확실한 효율 이득이 없어 OFF 유지.

10개 attribution에서 단일 원인으로 확정된 건 0개. Position 0은 king safety OFF와
TT OFF에서 reference move가 복구됐지만 여러 경로가 바뀌므로 UNKNOWN이다.
이는 현재 fresh-state replay이고 원 대국 history/TT를 재구성하지 않는다.
confidence는 숫자 확률이 아니라 `SINGLE_CONTROLLED_REPLAY_NOT_CAUSAL`로 표기한다.

1차 30 FEN profile 합계: legal 2,094.8ms, eval 2,078.7ms, order 1,224.5ms,
correction 205.4ms, TT probe 15.4ms. qsearch는 전체 600,000 nodes 중 536,965 nodes.
qsearch에서 16,924,268개 합법 수를 만들고 305,405개를 검색했다. 체크 수 검색
59,192회, checked node 71,070회, delta prune 39,959회, SEE prune 39,811회.
inclusive timer를 서로 더해 wall time으로 해석하면 안 된다. 특히 qsearch_ns는
재귀 포함 시간이라 CPU 구간별 합산 용도가 아니다.

최종 재측정(`*-final`)에서도 fixed-node/depth의 위 결과는 동일하다.
최종 profile은 legal 2,238.6ms, eval 2,168.4ms, order 1,271.2ms,
correction 225.3ms, TT 16.1ms였다. Partial ordering의 depth-4 wall 합계는
4,802 → 4,946ms로 개선되지 않았다. 고정 50ms에서 평균 depth는 2.667 → 2.700,
agreement는 둘 다 2/30이지만 시간 변동을 포함하므로 강도 개선의 증거가 아니다.

## TimeGuard / paired

10 FEN × 4 budgets(1/5/20/50ms) × OFF/ON = 80 CLI searches.
양쪽 engine-reported overshoot 최대 0ms, ON의 조기 정지는 32/40이었다.
이는 제한된 CLI smoke다. 별도 partial-ordering 50ms 실험의 FEN #26에서는
83ms(33ms overshoot)가 관측됐다. OS scheduling과 긴 내부 연산 중 무엇이 원인인지
분리하지 못했으며 모든 실행이 hard deadline 안에 끝났다고 주장하지 않는다.

0.5 vs 0.6 동일 Middlegame preset, Threads 1, Hash 32, proof 0, TC 1+0.01,
concurrency 2, 20-opening suite, 색 교대. TimeGuard와 adaptive_time은 양쪽 OFF.
20-game smoke: 8W/8L/4D, 양쪽 시간패/불법 수 0. SPRT 미결론.
Runner가 claim 가능한 3-fold 이후의 PV continuation을 경고한 사례는 보존했다.
이는 불법 수 집계와 구분하며, 해당 PV 표시 동작을 이번에 변경하지 않았다.

100-game pilot: **47W/36L/17D, 55.5%**, 양쪽 시간패/불법 수 0, runner 정상 종료.
Runner Elo estimate +38.37 ±63.05, LOS 88.81%, SPRT LLR +0.18,
경계 [-2.94, +2.94]로 **inconclusive**. TimeGuard 자체의 대국 검증이 아니며
어느 실험 기능도 자동 승격하지 않는다. 이 표본으로 0.4를 넘었다거나
production strength successor라고 부를 수 없다.

## 실행

```powershell
.\build.ps1 -Configuration Release -NativeAVX2
.\build\Release\axiom-0.6-attribution.exe --analyze --middlegame --proof-nodes 0 --time 0 --nodes 20000 --safety-mask no-king
.\build\Release\axiom-0.6-attribution.exe --analyze --middlegame --proof-nodes 0 --time 0 --depth 6 --force-root-move e2e4 --no-lmr
node tools/v06-experiments.mjs --mode safety --limit 30 --out results/my-new-safety-run
node tools/v06-experiments.mjs --mode holdout --limit 300 --out results/my-new-holdout
```

Output directory가 이미 존재하면 재사용하지 않는다. 실험별 manifest에 engine/corpus/
source file hashes, command, limits와 provenance를 기록한다. 첫 단계 자료는 그대로
보존하고 최종 재측정은 `*-final` 디렉터리로 분리한다.

## 미완료 / 다음 작업

1. 실제 전체 게임의 FIRST/LARGEST/ALL_THRESHOLD extraction. 기존 300개는 first
   **observed** qualifying sample이며 exhaustive extraction의 대체물이 아니다.
2. 수별 prune/reduction 상세 trace와 targeted leaf replay. 현재 root replay와 통계만으로
   STATIC_EVAL/PAWN_STRUCTURE/HORIZON/QSEARCH/FUTILITY를 자동 판정하지 않는다.
3. 모든 heuristic × safety reason의 실제 prevented prune / reduced plies / re-search
   효과 matrix. 현재 skipped eligible checks의 joint credit이며 causal node savings가 아니다.
4. legal generation 내부의 check/pin/evasion 및 evaluation term별 세부 profiling.
   현재 legal/eval/order 등의 전체 타이머와 qsearch 상세 카운터 수준이다.
5. 완전한 staged MovePicker. 이번 partial sort 후보는 OFF로 남긴다.
6. TimeGuard의 실전 clock stress / OS stall 및 long-operation 분해. CLI deadline smoke만으로
   시간패 안전성을 입증할 수 없다. adaptive_time/calibrated_lmr/rfp는 OFF 유지.
7. 더 큰 독립 corpus, 다중 frozen state, 낮은 예산뿐 아니라 높은 예산의 replay와
   충분한 paired/SPRT. Top-3/current CP-loss는 새 선택 수별 reference 재평가가 필요하다.

다음 가장 유망한 검증 후보는 improving/tactical 보호의 선택적 약화다. 작은 표본의
agreement 차이만으로 기본값을 변경하지 않는다.
