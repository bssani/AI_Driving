# Racing AI

언리얼 엔진용 레이싱 AI 상대 차량 플러그인입니다.
스플라인 레이싱 라인 추종, 곡률 기반 속도 제어와 자동 제동 지점, 추월 중재,
러버밴딩(따라잡기 보정), 랩·순위 집계를 제공합니다.

**사용 설명서: [`Docs/index.html`](Docs/index.html)** — 브라우저로 열어 주세요.

## 구성

| 모듈 | 의존 | 내용 |
|---|---|---|
| `RacingAI` | Core, CoreUObject, Engine | AI 로직 전부. 차량 물리에 의존하지 않습니다 |
| `RacingAIChaos` | + ChaosVehicles | Chaos 차량 연결 어댑터만 |

코어 모듈이 차량 물리를 모르기 때문에, Chaos가 아닌 물리로 옮길 때
교체 대상은 `RacingAIChaos` 하나뿐입니다.

## 빠른 시작

1. 이 폴더를 프로젝트의 `Plugins/`에 복사하고 프로젝트를 다시 빌드
2. 레벨에 **Racing Spline**을 놓고 트랙을 따라 점을 찍은 뒤 **Closed Loop** 체크
3. **Race Grid Spawner**를 놓고 `Vehicle Class`(Pawn)와
   `Vehicle Input Adapter Class`(`ChaosVehicleInputAdapter`)를 지정
4. `Grid Slots`에 AI 대수만큼 칸을 추가하고 난이도 프로필을 배정
5. `Draw Debug`를 켜고 Play

## 기록 시간

화면에 띄우고 기록으로 남길 숫자는 `GetPlayerTimeSeconds()`입니다.

    Director->GetPlayerTimeSeconds()   달리는 동안 올라가고, 결승선을 넘는 순간 멈춘다
    Director->IsPlayerTimeRunning()    아직 움직이는지. 꺼지는 순간이 기록 확정
    PlayerParticipant->bFinished       기록으로 쓸 수 있는지

**`GetRaceElapsedSeconds()`를 그대로 쓰면 안 됩니다.** 그것은 레이스의 길이지 플레이어의
기록이 아닙니다. 뒤에 오는 AI를 기다리는 동안에도 계속 올라가므로, 완주한 사람의 숫자가
결과 화면에서 혼자 늘어납니다. 실측으로 확인한 값입니다 — 선두가 35.60초에 완주한 뒤
레이스가 76초까지 도는 동안 그의 `FinishTimeSeconds`는 35.60에 그대로 있었습니다.

`GetRaceElapsedSeconds()`도 레이스가 끝나면 그 시점에서 멈춥니다. 깃발이 내려간 뒤로도
세는 시계는 무엇의 경과 시간도 아닙니다.

미완주면 레이스가 끝난 시각이 나옵니다. 0이 아닌 이유는 결과 화면이 00:00으로 튀면
고장처럼 보이기 때문이고, 그것이 기록이 아니라는 사실은 `bDidNotFinish`가 말합니다.

## 반드시 해야 하는 것

자동차를 바꿀 때마다 **횡가속 한계를 실측해서** 프로필의
`Lateral Accel Budget`과 `Min Turn Radius`를 다시 잡아야 합니다.
방법은 설명서 6단계에 있습니다. 건너뛰면 AI가 지나치게 느리거나
코너에서 트랙 밖으로 밀려납니다.

## 포함된 콘텐츠

- `Content/Profiles/DA_Driver_{Rookie,Advanced,Pro}` — 난이도 프로필
- `Content/PM_TrackSurface` — 노면 물리 머티리얼
- `Content/Maps/TestOval` — 697m 테스트 오벌 트랙

테스트 맵은 언리얼 Vehicle 템플릿의 `BP_SportsCar_Pawn`과
`BP_VehicleAdvGameMode`를 참조합니다. 다른 프로젝트에서는 이 두 곳만 바꾸면 됩니다.
플러그인의 나머지 콘텐츠는 외부 의존이 없습니다.
