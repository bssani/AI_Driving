# AI 난이도와 코너 조정

AI 운전사의 난이도는 성격표라고 부르는 데이터 에셋 세 장으로 정한다.
`Plugins/RacingAI/Content/Profiles/`의 `DA_Driver_Rookie`, `DA_Driver_Advanced`, `DA_Driver_Pro`다.

어느 AI가 어느 성격표를 쓸지는 레벨의 `RaceGridSpawner`에서 그리드 슬롯마다 `Profile` 칸으로 정한다.
`bRandomizeProfiles`를 켜면 대신 `ProfilePool`에서 무작위로 고른다.

AI가 어떤 원리로 길을 따라가는지는 [AI_HowItDrives.md](AI_HowItDrives.md)에 쉬운 말로 풀어 두었다.
아래 표의 값은 2026-09-13 기준 에셋에 들어 있는 값이다. 값이 하나뿐인 칸은 세 성격표 공통이다.

---

## 1. 코너에서 벽에 박을 때

### 왜 박나

AI는 코너 앞에서 언제 브레이크를 밟을지 스스로 계산한다.
앞 구간이 얼마나 휘었는지로 코너 통과 속도를 구하고, 차가 `BrakingDecel`만큼 감속할 수 있다고 가정해
제동 시작 지점을 거꾸로 잡는다. 이 가정이 차의 실제 제동력보다 크면 늦게 밟는다.

150 km/h에서 80 km/h로 줄여야 하는 코너에서, 차가 실제로는 0.9 G만 낸다고 **가정한** 예시다.

| 항목 | 값 |
|---|---|
| AI가 계산한 제동 거리. Pro 설정 1300 cm/s², 약 1.33 G | 48 m |
| 차가 실제로 필요한 제동 거리. 0.9 G | 70 m |
| 48 m 앞에서 밟기 시작했을 때 코너 진입 속도 | 108 km/h |

80 km/h로 들어가야 할 코너에 108 km/h로 들어간다.

### 원인 가르기

`RaceGridSpawner`의 `bDrawDebug`를 켜면 AI 머리 위에 다음 줄이 뜬다.

```
P순위 L랩/총랩  현재속도/목표속도 km/h  scale 러버밴딩배율  상태
```

코너에 들어가기 직전에 현재 속도와 목표 속도를 본다.

| 코너 직전 표시 | 뜻 | 고칠 값 |
|---|---|---|
| 140/90처럼 현재가 목표보다 한참 높음 | 줄여야 하는 건 알지만 제때 못 줄임 | `BrakingDecel` 내리기, `SpeedControlGain` 올리기 |
| 두 숫자가 비슷한데 목표 자체가 높음 | 코너를 실제보다 완만하게 봄 | `LateralAccelBudget`이나 `SpeedMargin` 내리기, 스플라인 점 확인 |
| 속도는 맞는데 바깥으로 밀려남 | 핸들을 덜 꺾음 | `MinTurnRadius` 내리기, `MaxSteeringRate` 올리기 |

### Pro에 처음 넣어볼 값

위 표에서 해당하는 줄만, 한 번에 하나씩 바꾼다.

| 값 | 지금 | 첫 시도 |
|---|---|---|
| `BrakingDecel` | 1300 | 950 |
| `SpeedControlGain` | 0.004 | 0.008 |
| `LateralAccelBudget` | 1413 | 1200 |
| `SpeedMargin` | 0.96 | 0.92 |
| `MinTurnRadius` | 4000 | 3000 |

### 알아둘 것

- **`SpeedControlGain`** 은 브레이크가 얼마나 일찍 끝까지 들어가는지다. 0.004면 목표보다 9 km/h 넘게 빨라야 브레이크를
  끝까지 밟고, 0.008이면 4.5 km/h부터 끝까지 밟는다. 너무 올리면 직선에서 가속과 감속을 번갈아 하며 울컥거린다.
- **곡률은 `RacingSpline`으로 계산한다.** 스플라인 메시는 보지 않는다. 코너에 점이 적으면 스플라인이 둥글게 이어져
  실제 도로보다 완만하게 계산된다. 코너에는 점을 촘촘히 두고 도로 중앙을 따라가게 한다. 급한 코너가 많으면
  그 액터의 `CurvatureSampleStep`을 200에서 100으로 줄인다.
- **코너 속도는 중심선 기준이다.** 추월하느라 안쪽 차선에 있으면 실제 반경이 더 작아 조금 더 빨리 들어간다.
  추월 중에만 박는다면 이것이다.
- **`BrakingDecel`, `LateralAccelBudget`, `MinTurnRadius`는 원래 템플릿 차량에서 잰 값이다.**
  차를 바꿨다면 `racing.Calibrate`로 지금 차를 재면 추측 없이 정확한 숫자가 나온다. 방법은 [Tuning.md](Tuning.md) 0장.

---

## 2. 난이도가 드러나는 곳

같은 반지름 50 m 커브를 러버밴딩 없이 지날 때의 속도다.
코너 속도는 `LateralAccelBudget`과 반경을 곱한 값의 제곱근에 `SpeedMargin`을 곱해서 나온다.

| 성격표 | 커브 속도 |
|---|---|
| Rookie | 약 70 km/h |
| Advanced | 약 81 km/h |
| Pro | 약 92 km/h |

---

## 3. 속성별 의미

값 칸은 Rookie / Advanced / Pro 순서다.

### 속도

난이도 차이가 가장 크게 드러나는 곳이다.

| 값 | 의미 | 지금 값 | 어렵게 하려면 |
|---|---|---|---|
| `LateralAccelBudget` | 코너에서 허용하는 옆 방향 가속도 cm/s². 코너 속도가 이 값의 제곱근에 비례 | 929 / 1171 / 1413 | 올린다. 차 한계를 넘기면 빨라지지 않고 박는다 |
| `BrakingDecel` | 코너 앞 제동 시작 지점을 계산할 때 쓰는 감속도 cm/s² | 900 / 1100 / 1300 | 올린다. 이것도 차 한계까지만 |
| `SpeedMargin` | 모든 목표 속도에 곱하는 배율 | 0.90 / 0.93 / 0.96 | 1에 가깝게 |
| `MaxSpeed` | 직선 최고 속도 cm/s. 0.036을 곱하면 km/h | 5000 / 6000 / 8600 | 올린다 |
| `SpeedControlGain` | 속도 차이를 스로틀과 브레이크 입력으로 바꾸는 비율 | 0.004 | 난이도와 무관 |

### 조향

| 값 | 의미 | 지금 값 | 어렵게 하려면 |
|---|---|---|---|
| `LookaheadSeconds` | 몇 초 뒤에 도착할 지점을 겨냥할지 | 1.00 / 0.85 / 0.72 | 줄인다. 너무 줄이면 핸들이 떨린다 |
| `MaxLookaheadAngleDegrees` | 코너에서 겨냥점이 줄을 따라 이 각도보다 더 돌아가 있으면 겨냥 거리를 줄인다 | 30 | 코너 안쪽으로 파고들면 줄인다 |
| `MaxSteeringRate` | 1초에 핸들을 꺾을 수 있는 양. 2.5면 끝까지 꺾는 데 0.4초 | 2.2 / 2.6 / 3.0 | 올린다 |
| `MinTurnRadius` | 차량 특성. 주행 속도에서 핸들을 끝까지 꺾었을 때 그리는 원의 반지름 cm | 4000 | 난이도와 무관 |
| `SteeringGain` | 조향 입력에 마지막으로 곱하는 배율 | 1.0 | 난이도와 무관 |
| `MinLookahead`, `MaxLookahead` | 겨냥 거리의 최소와 최대 cm | 400, 4000 | 보통 그대로 |
| `LateralOffsetRate` | 차선을 옮기는 속도 cm/s | 300 | 보통 그대로 |
| `RacingLineOffset` | 방해가 없을 때 돌아갈 자리. 중심선 기준, 오른쪽이 양수 | 0 | 코스에 빠른 라인이 따로 있으면 옮긴다 |
| `GridLaneHoldSeconds` | 출발 후 그리드 차선에서 레이싱 라인으로 옮겨 가는 시간 | 5 | 보통 그대로 |

### 추월

| 값 | 의미 | 지금 값 | 어렵게 하려면 |
|---|---|---|---|
| `OvertakeSpeedRatio` | 앞차가 내 속도에 이 값을 곱한 것보다 느릴 때만 추월을 시도 | 0.85 / 0.93 / 0.97 | 1에 가깝게. 조금만 느린 차도 추월한다 |
| `FollowGap` | 같은 차선 앞차와 유지하는 간격 cm | 900 / 750 / 600 | 줄인다 |
| `OvertakeLateralOffset` | 추월할 때 옆으로 비켜서는 거리 cm | 220 / 250 / 280 | 트랙이 좁으면 줄인다 |
| `OvertakeScanDistance` | 앞차를 알아채기 시작하는 거리 cm | 3000 | 보통 그대로 |
| `PassingLateralClearance` | 좌우로 이만큼 떨어져 있으면 다른 차선으로 본다 cm | 240 | 차 폭보다 조금 넉넉하게 |
| `MinOvertakeSpeed` | 추월하려는 중에 유지할 최소 속도 cm/s | 650 | 그대로 |
| `HardStopGap` | 같은 차선에서 이보다 가까우면 추월 의사와 상관없이 멈춘다 cm | 420 | 그대로 |

### 러버밴딩

행사장에서 느끼는 난이도는 사실상 여기서 정해진다.

| 값 | 의미 | 지금 값 | 어렵게 하려면 |
|---|---|---|---|
| `CatchUpStrength` | 보정 전체의 세기. 0이면 끈다 | 0.85 / 0.60 / 0.35 | 줄인다 |
| `MaxSlowDown` | 플레이어보다 앞설 때 속도를 낮추는 비율의 상한 | 0.28 / 0.20 / 0.08 | 줄인다. 0이면 전혀 봐주지 않는다 |
| `MaxSpeedUp` | 뒤처졌을 때 속도를 올리는 비율의 상한 | 0.18 / 0.15 / 0.10 | 올린다 |
| `NeutralGap` | 플레이어와 이 거리 안이면 보정하지 않는다 cm | 1500 | 그대로 |
| `FullEffectGap` | 보정이 최대로 걸리는 거리 cm | 12000 | 그대로 |

**`MaxSlowDown`과 `MaxSpeedUp`은 그대로 적용되지 않는다.** `CatchUpStrength`를 곱한 값이 실제 최대치다.
`NeutralGap`에서 `FullEffectGap`까지 거리에 비례해 커진다.

| 성격표 | 앞설 때 실제 최대 | 뒤처질 때 실제 최대 |
|---|---|---|
| Rookie | 23.8% 느려짐 | 15.3% 빨라짐 |
| Advanced | 12% 느려짐 | 9% 빨라짐 |
| Pro | 2.8% 느려짐 | 3.5% 빨라짐 |

### 플레이어 배려

| 값 | 의미 | 지금 값 | 어렵게 하려면 |
|---|---|---|---|
| `PlayerExtraClearance` | 플레이어 옆을 지날 때 더 벌리는 거리 cm | 220 / 160 / 120 | 줄인다. VR에서 너무 붙으면 멀미가 날 수 있다 |
| `PlayerYieldDistance` | 앞차가 플레이어이고 이 거리 안이면 더 벌린다 cm | 1500 | 그대로 |

### 복구

갇히거나 뒤집혔을 때 쓰는 값이다. 난이도와는 관계없다.

| 값 | 의미 | 지금 값 |
|---|---|---|
| `StuckProgressSpeed` | 줄을 따라 이 속도보다 느리게 나아가면 갇힘 타이머가 돈다 cm/s | 60 |
| `StuckTimeToReverse` | 갇힘으로 판정하기까지의 시간 초 | 1.5 |
| `LaunchGraceSeconds` | 출발과 재배치 직후 갇힘 판정을 쉬는 시간 초 | 3 |
| `ReverseDuration` | 후진을 유지하는 시간 초 | 1.2 |
| `MaxStuckAttempts` | 후진 탈출을 몇 번까지 시도할지. 넘기면 재배치 | 2 |
| `FlippedAngleDegrees`, `FlippedTimeToRespawn` | 이 각도 넘게 기운 채 이 시간이 지나면 재배치 | 70도, 2.5초 |
| `OffTrackRespawnWidthScale` | 줄에서 도로 반폭의 몇 배보다 멀어지면 재배치 | 1.5 |
| `RespawnAheadDistance` | 재배치할 때 원래 지점보다 얼마나 앞에 놓을지 cm | 0 |

---

## 4. 난이도를 올리는 순서

1. **러버밴딩부터.** `MaxSlowDown`과 `CatchUpStrength`를 줄인다. 코너 한계를 건드리지 않으므로 벽에 박을 위험이 없다.
2. **`SpeedMargin`과 `MaxSpeed`를 올린다.**
3. **`LookaheadSeconds`를 줄이고 `MaxSteeringRate`를 올린다.** 라인을 더 정확히 물게 된다.
4. **`LateralAccelBudget`과 `BrakingDecel`은 마지막.** 차의 한계를 넘지 않는 선까지만 올린다.
   넘기면 AI가 빨라지지 않고 코너에서 박는다. 한계는 `racing.Calibrate`로 잰다.
