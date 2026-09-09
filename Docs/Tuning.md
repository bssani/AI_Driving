# 조정 가이드

차량 속도, AI 난이도, 차량 교체, 색상. 값이 어디 있고 무엇이 무엇을 바꾸는지 정리한다.
숫자는 전부 2026-09-10에 이 프로젝트에서 실측한 것이고, 출처를 밝히지 않은 값은 없다.

---

## 0. 먼저 재라

프로파일에 든 숫자 중 셋은 난이도가 아니라 **그 차량의 물리적 한계**다. 차를 바꾸면
그대로 다른 차의 값이 되고, 증상은 "AI가 이상하다"로 나타난다.

| 값 | 뜻 | 안 맞으면 |
|---|---|---|
| `MinTurnRadius` | 주행 속도대에서 풀락으로 그린 원의 반지름 | 실제보다 크면 조향 게인이 모자라 코너 바깥으로 밀려 벽을 긁는다 |
| `LateralAccelBudget` | 그 차가 낼 수 있는 횡가속의 몇 % | 낮으면 코너에서 기어가고, 높으면 물고 들어가다 밀려난다 |
| `BrakingDecel` | 실측 제동 감속 | 낮으면 일찍 제동해 느리고, 높으면 코너 진입에서 넘친다 |

재는 것은 콘솔 두 줄이다. 사람이 몰기만 하면 되므로 조작 방식과 무관하다. 스티어링 휠로
재도 된다.

```
racing.Calibrate 1     시작
                       직선에서 풀 스로틀       -> 최고 속도, 0-100
                       달리다 풀 브레이크        -> 제동 감속
                       넓은 곳에서 풀락으로 몇 바퀴 -> 횡가속 한계, 유효 선회 반경
racing.Calibrate 0     결과와 세 프로파일 권장값 출력
```

`racing.CalibrateReport`는 측정을 멈추지 않고 중간 결과만 본다.

> **에디터를 반드시 앞으로 꺼내고 재라.** 백그라운드에서는 3fps로 돌아 값이 전부 무의미해진다.
> 스크립트로 폴링하면서 재도 안 된다. 폴링이 프레임 시간을 부풀려 가속도를 10분의 1로 만든다.
> 실제로 그렇게 쟀다가 최대 가속 0.14G, 0-100 34초라는 값을 얻은 적이 있다.

---

## 1. 차량 속도

값은 두 군데에 있고 **블루프린트가 이긴다.**

- C++ 기본값: `Source/AI_Driving/AI_DrivingSportsCar.cpp`, 바퀴는 `SportsCar/` 아래
- 실제로 쓰이는 값: `Content/VehicleTemplate/Blueprints/SportsCar/BP_SportsCar_Pawn`의
  Vehicle Movement 컴포넌트

### 템플릿 스포츠카 실측값

| 항목 | 값 |
|---|---|
| 질량 (무브먼트 컴포넌트) | 1500 kg |
| 최대 토크 | 750 Nm |
| 토크 곡선 최고점 | 3000 rpm에서 0.971 → 728 Nm |
| 6000 rpm 토크 | 600 Nm → 약 505 hp |
| 최대 / 아이들 RPM | 7000 / 900 |
| 전진 기어 | 4.25 / 2.52 / 1.66 / 1.22 / 1.00 |
| 최종 감속 | 2.81 |
| 변속 시점 | 상 6000 rpm, 하 2000 rpm, 0.2초 |
| 항력 계수 | 0.31 |
| 바퀴 반지름 | 앞 39 cm, 뒤 40 cm |
| 타이어 마찰 배수 | 앞 3.0, 뒤 4.0 |
| 최대 조향각 | 40도 (앞바퀴) |

토크 곡선은 별도 에셋이다. `Content/VehicleTemplate/Blueprints/SportsCar/FC_SportsCar_Torque`.
곡선 값은 0~1 배수이고 `MaxTorque`에 곱해진다.

### Corvette 기준으로 맞추려면

참고 제원(측정값 아님): C8 Stingray는 1530 kg, 637 Nm, 495 hp, 0-100 약 2.9초, 최고 312 km/h.

출력 대 질량만 보면 템플릿 차량이 이미 Corvette급이다(505 hp / 1500 kg). 그래도 느리게
느껴진다면 토크가 아니라 아래를 먼저 본다.

- **기어비.** 5단뿐이라 단 사이가 넓다. 변속 후 회전수가 크게 떨어져 가속이 끊긴다
- **타이어 마찰.** 마찰이 모자라면 토크를 아무리 올려도 바퀴만 헛돈다. 0-100이 유독
  나쁘면 여기다
- **변속 시점.** `ChangeUpRPM`이 6000인데 이 곡선은 6000에서 이미 0.8로 내려와 있다.
  최고 출력 지점 근처에서 올리는 것이 맞다
- **질량.** 무브먼트 컴포넌트의 `Mass`가 물리 시뮬레이션이 쓰는 값이다

고친 뒤에는 `racing.Calibrate`로 0-100과 최고 속도를 재서 목표(2.9초 / 312 km/h)와 비교한다.

---

## 2. AI 난이도

프로파일 세 개가 `Plugins/RacingAI/Content/Profiles/`에 있다.

### 지금 값

| | Rookie | Advanced | Pro |
|---|---|---|---|
| `LateralAccelBudget` | 929 | 1171 | 1413 |
| `BrakingDecel` | 900 | 1100 | 1300 |
| `SpeedMargin` | 0.90 | 0.93 | 0.96 |
| `MaxSpeed` (cm/s) | 5000 | 6000 | **8600** |
| `LookaheadSeconds` | 1.00 | 0.85 | 0.72 |
| `MaxSteeringRate` | 2.2 | 2.6 | 3.0 |
| `CatchUpStrength` | 0.85 | 0.60 | **0.35** |
| `MaxSlowDown` | 0.28 | 0.20 | **0.08** |
| `MinTurnRadius` | 4000 | 4000 | 4000 |

굵은 값은 2026-09-10에 올린 것이다. `LateralAccelBudget`은 손대지 않았다. 옛 템플릿
차량에서 잰 2.06 G의 46 / 58 / 70%인데, 차를 바꿨다면 이미 다른 차의 값이므로
**올리기 전에 0장을 먼저 하라.**

### Pro를 더 빠르게 만들려면

순서가 있다. 거꾸로 하면 벽을 긁는다.

1. **`racing.Calibrate`로 재고** `LateralAccelBudget`, `BrakingDecel`, `MinTurnRadius`를
   그 차 값으로 바꾼다. 이게 없으면 나머지는 전부 추측이다
2. `SpeedMargin`을 올린다. 0.96 → 0.98이면 코너를 한계에 더 가깝게 돈다
3. `MaxSpeed`를 올린다. 직선 상한이라 차가 그만큼 못 내면 아무 일도 안 일어난다
4. `MaxSlowDown`을 줄인다. 앞서 있을 때 봐주는 정도다. 0이면 전혀 안 봐준다
5. `LookaheadSeconds`를 줄인다. 반응이 빨라지지만 너무 줄이면 조향이 떨린다

### 빠르게 만들면 벽을 긁는 이유

조향은 pure pursuit이다. 앞쪽 한 점을 정해 그리로 **직선으로** 겨눈다. 주시 거리는
속도 × `LookaheadSeconds`라서 빨라질수록 멀어지고, 굽은 구간에서 그 직선은 코스 안쪽을
가로지른다. 즉 빨라질수록 코너를 크게 잘라먹는다.

`MaxLookaheadAngleDegrees`(기본 30도)가 이걸 막는다. 목표점이 코스를 따라 몇 도나 돌아간
자리인지로 주시 거리를 제한한다. 직선에서는 곡률이 0이라 아무 영향이 없고 코너에서만 짧아진다.
코너에서 안쪽으로 파고들면 줄이고, 반응이 굼뜨면 늘린다.

### 러버밴딩

행사장에서는 이쪽이 체감 난이도를 더 크게 좌우한다. 일반 관람객이 전력으로 달리는 AI를
따라잡을 수는 없다.

- `CatchUpStrength` 0이면 끔
- `MaxSpeedUp` 뒤처졌을 때 허용할 속도 상향 비율
- `MaxSlowDown` 앞섰을 때 허용할 속도 하향 비율
- `NeutralGap` 이 거리 안에서는 보정 안 함. 접전 구간을 흔들지 않기 위한 것

### 확인된 동작 (2026-09-10, 벽 세운 TestOval, 5랩)

| | 랩 | 결과 | 최종 좌우 오프셋 |
|---|---|---|---|
| Pro | 5 | 134.81초 완주 | 5 cm |
| Advanced | 4 | 시간 초과 | 245 cm |
| Rookie | 3 | 시간 초과 | -11 cm |

재배치 0회, 후진 탈출 0회. 벽이 ±700 cm에 있는데 오프셋이 한 자리 수라는 것은 라인
한가운데를 물고 달렸다는 뜻이다.

---

## 3. 차량 교체

바꿀 곳은 두 군데다.

- **AI 차량**: 레벨의 `RaceGridSpawner` → `Vehicle Class`.
  그리드 자리마다 다른 차를 쓰려면 각 슬롯의 `Vehicle Class Override`
- **플레이어 차량**: 게임모드의 Default Pawn Class

### 새 차량이 갖춰야 할 것

1. **Chaos 휠 차량 폰**이어야 한다
2. **`Chaos Vehicle Input Adapter` 컴포넌트**가 붙어야 AI가 몰 수 있다. 스포너의
   `Vehicle Input Adapter Class`에 지정해 두면 스폰할 때 자동으로 붙는다.
   이 어댑터는 Chaos의 두 가지 기본 동작을 꺼 준다. 컨트롤러가 빙의해야 입력을 받는
   요구(AI 차량은 아무도 빙의하지 않는다)와, 브레이크를 후진으로 해석하는 아케이드 동작
   (AI가 감속할 때마다 후진하게 된다). 둘 다 켜 두면 차가 아예 안 움직이거나 뒤로 간다
3. **바퀴 본 이름**이 차량 클래스의 `WheelSetups`와 맞아야 한다. 템플릿은
   `Phys_Wheel_FL` / `FR` / `BL` / `BR`이다
4. **`Race Participant` 컴포넌트**. AI 차량에는 스포너가 붙여 준다. 플레이어 차량에는
   직접 붙이고 `Is Player`를 켠다

### 바꾼 다음에

**반드시 `racing.Calibrate`로 다시 재라.** 프로파일의 세 값이 전부 옛 차량 기준이 된다.
이 단계를 건너뛰면 AI가 느려지거나 벽을 긁는데, 원인이 AI에 있는 것처럼 보여 엉뚱한 데를
고치게 된다.

---

## 4. 차량 색상

### 먼저 구성을 본다

```
racing.DumpMaterialSlots
```

플레이어 차량의 모든 메시와 슬롯, 지금 발린 머티리얼과 그 베이스를 찍는다.
템플릿 스포츠카는 이렇게 나온다.

```
  Chassis_Main  (StaticMeshComponent, 슬롯 2개)
    [0] M_SportsCarChasis      M_SportsCarChasis  (베이스 M_SportsCarBase)
    [1] MI_SportsCarBody       MI_Livery_White    (베이스 M_SportsCarBase)
  Wheel_FL      (StaticMeshComponent, 슬롯 2개)
    [0] M_SportsCar_Wheel      M_SportsCar_Wheel
    [1] MI_SportsCar_Tire      MI_SportsCar_Tire
  ...
```

차체 도색은 `Chassis_Main`의 1번 슬롯 하나뿐이고, 슬롯 이름은 메시마다 제각각이다.
이게 "메시가 여러 개고 슬롯이 제각각"인 상태다.

### 방법 1 — 단순한 차량

스포너의 `Livery Mesh Component Name`과 `Livery Material Slot`을 지정한다.
메시 하나에 슬롯 하나면 이걸로 충분하다.

### 방법 2 — 메시가 여러 개일 때 (권장)

차량 폰에 **`Racing Livery Applier` 컴포넌트**를 붙인다. 스포너는 차량이나 그 컴포넌트가
`IRacingVehicleLivery`를 구현하고 있으면 도색을 통째로 맡기므로, 붙이기만 하면 된다.

찾는 방법이 둘이고 **둘 중 하나만 맞아도 칠한다.**

- **`Paint Slot Names`** — 슬롯 이름으로 찾는다. 이름이 분명하면 이게 가장 정확하다.
  템플릿 차량은 `MI_SportsCarBody` 하나면 된다
- **`Paint Materials`** — 지금 발린 머티리얼의 **베이스가 같은** 슬롯을 전부 찾는다.
  슬롯 이름이 제각각이거나 메시가 여러 개로 쪼개져 있을 때 쓴다.
  주의: 위 예에서 `M_SportsCarBase`를 넣으면 0번 슬롯까지 같이 잡힌다. 베이스를 공유하는
  슬롯이 여럿이면 이름 쪽을 쓰거나 둘을 섞어 쓴다

그 밖에:

- **`Color Parameter Name`** (기본 `BodyColor`) — 도색의 `TintColor`를 이 이름의 벡터
  파라미터에 넣는다. 머티리얼에 그 파라미터가 없으면 조용히 지나간다. 파라미터를 하나
  두면 색이 몇 개든 머티리얼 에셋은 하나로 끝난다
- **`Replace Material`** — 도색에 머티리얼이 지정돼 있으면 슬롯을 그것으로 교체한다.
  끄면 색만 바꾼다. 데칼이나 무광/유광까지 달라지는 도색이면 켠다
- **`Mesh Component Names`** — 비우면 모든 메시를 훑는다. 바퀴처럼 실수로 칠하면 곤란한
  메시가 같은 머티리얼을 쓴다면 여기서 좁힌다
- **`Log Applied`** — 무엇을 칠했는지 로그로 남긴다. 처음 붙일 때 켜 두면 대상 확인이 쉽다

### 자식 블루프린트는 권하지 않는다

색깔 수만큼 에셋이 늘고, 차량 설정을 고칠 때마다 전부 손봐야 하며, 무엇보다 런타임에
색을 고를 수 없어 그리드마다 색을 섞을 수 없다.

### 도색 목록

`Plugins/RacingAI/Content/Liveries/DA_DefaultLiveries`에 넷이 있다.
RacingRed / RacingBlue / RacingYellow / RacingWhite. 각각 머티리얼과 대표색을 갖는다.
대표색은 머티리얼을 읽을 수 없는 곳(순위표 색점, 관중 화면)에서 같은 차를 같은 색으로
표시하려고 따로 들고 있는 값이다.

배정은 스포너가 한다. 그리드 슬롯의 `Livery Name`을 비워 두면 무작위로 고르고,
`Avoid Duplicate Liveries`가 켜져 있으면 한 레이스에 같은 색이 겹치지 않는다.
`Random Seed`를 0이 아닌 값으로 두면 매번 같은 결과가 나온다.

### 확인된 동작 (2026-09-10)

`Paint Slot Names = [MI_SportsCarBody]`로 두고 4대를 스폰했다.

```
BP_SportsCar_Pawn_C_0  RacingWhite   Chassis_Main[1] = MID_MI_Livery_White_0
BP_SportsCar_Pawn_C_1  RacingRed     Chassis_Main[1] = MID_MI_Livery_Red_0
BP_SportsCar_Pawn_C_2  RacingYellow  Chassis_Main[1] = MID_MI_Livery_Yellow_0
BP_SportsCar_Pawn_C_3  RacingBlue    Chassis_Main[1] = MID_MI_Livery_Blue_0
```

차마다 다른 색, 슬롯 하나만 정확히. 바퀴, 유리, 손은 건드리지 않았다.

---

## 5. 충돌 소리와 스파크

### 접촉은 매 프레임 보고된다

이것이 이 영역의 모든 문제의 뿌리다. 벽을 한 번 스치는 사건이 끝날 때까지 매 프레임
보고되므로, 프레임마다 반응하면 한 번의 충돌이 초당 여덟 번의 소리와 프레임마다 하나씩의
스파크가 된다.

- **소리**: `ImpactEscalation`(이번 접촉에서 가장 컸던 것보다 이 배수 이상 커야 다시 울림,
  기본 1.6)과 `ContactReleaseTime`(이만큼 끊겨야 새 접촉, 기본 0.25초)으로 묶는다.
  긁는 소리는 처음 닿을 때 한 번만 나고, 긁다가 제대로 박으면 그건 들린다
- **스파크**: `ScrapeInterval`(기본 0.12초)로 묶는다

### 들을 만한 것과 볼 만한 것의 기준은 다르다

충돌음은 **면으로 파고드는 속도**로 판정하는 것이 맞다. 그런데 불꽃은 짓눌림이 아니라
갈림에서 난다. 같은 기준을 쓰면 벽을 스치는 차는 아무것도 못 낸다. 벽을 따라 미끄러질 때
파고드는 속도는 거의 0이기 때문이다. 그러면 정면으로 받는 것만 튀게 되어 사실상 차끼리
부딪힐 때만 난다.

`Vehicle Impact FX` 컴포넌트는 접선 속도도 함께 본다.

| 값 | 뜻 | 기본 |
|---|---|---|
| `MinImpactSpeed` | 정면 충돌 문턱 (cm/s) | 250 |
| `MinScrapeSpeed` | 긁힘 문턱, 면을 따라 미끄러지는 속도 | 300 |
| `ScrapeInterval` | 긁는 동안 스파크 간격 (초) | 0.12 |
| `Severity Parameter Name` | 세기를 넣을 나이아가라 float 유저 파라미터 | `Severity` |

### 스파크 모양

`Content/VehicleFX/NS_ImpactSparks`가 붙어 있다. 엔진의 `DirectionalBurst` 템플릿을
복제한 것이라 접촉 지점에서 터지기는 하지만 아직 하얀 스프라이트 뭉치다.
나이아가라 에디터에서 손볼 곳은 넷이다.

1. 스프라이트 렌더러의 Alignment를 **Velocity Aligned**로. 불똥이 늘어져 날아가는 느낌
2. 머티리얼을 **가산(additive)** 계열로, 색은 주황에서 노랑
3. **Lifetime 0.3~0.6초**로 줄이고 Gravity Force와 Drag 추가
4. `Severity`라는 float 유저 파라미터를 만들어 Spawn Burst 개수에 물린다.
   컴포넌트가 매번 그 값을 넣어 준다

### 긁는 소리

`MS_ScrapeSound`는 실제로 재생된다. 벽을 세우자마자 들리기 시작해서 이번 데모에서는
`DA_SportsCar_Dynamic`의 `ScrapeSound`를 비웠다. 되살리려면 그 칸에 다시 넣으면 되고,
그 전에 MetaSound가 `Speed` / `Volume` 입력을 제대로 받는지부터 확인할 것.

---

## 6. 출발 전 잠금

`RaceDirectorSubsystem`의 `bLockPlayerUntilStart`가 켜져 있으면 출발 신호 전까지
사람이 차를 움직일 수 없다.

폰의 입력을 끄는 것만으로는 새어 나간다. `APawn::DisableInput`은 `bInputEnabled`를 내릴
뿐이고, 엔진이 입력 스택을 세울 때 빠지는 것은 그 폰의 입력 컴포넌트 하나다. 플레이어
컨트롤러 자신의 입력 컴포넌트는 그대로 남으므로, **스티어링 휠처럼 컨트롤러 쪽에 바인딩된
조작은 잠금을 그냥 통과한다.**

그래서 잠긴 동안에는 매 틱 차량을 직접 붙잡는다. 속도를 0으로 누르고, `LockedDriftTolerance`
(기본 30 cm)만큼 밀리면 제자리로 되돌린다. 무브먼트 컴포넌트에 풀 스로틀을 직접 넣어도
움직이지 않는 것을 확인했다.
