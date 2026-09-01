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
