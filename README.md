# AI_Driving

DIFA / Family Day 행사에서 보여줄 VR 레이싱 데모의 검증용 프로젝트다. 여기서 만든 것을
본 프로젝트로 옮긴다.

범위는 셋이다. **AI 상대차**(RacingAI 플러그인), **VR 및 VR 핸드**, **차량 사운드**
(VehicleSoundSystem 플러그인). 실제 휠(Thrustmaster T248) 연동은 본 프로젝트 소관이라
여기서는 다루지 않는다.

- UE 5.7.4
- Quest 2 / Quest 3, PC VR (Quest Link)
- 맵: `TestOval`(RacingAI, AI 차량 3대), `VehicleBasic`(차량 템플릿 평지)

## 플러그인

| 플러그인 | 저장소 | 역할 |
|---|---|---|
| `RacingAI` | 프로젝트 안 | AI 주행, 레이스 진행, 추돌 회피 |
| `VehicleSoundSystem` | [별도 저장소](https://github.com/bssani/VehicleSoundSystem) | 엔진·타이어·충돌음 |
| `MetaXR` | Meta 배포 | 핸드 트래킹 |
| `UnrealAgent` | 별도 | 에디터 자동화(개발용) |

`VehicleSoundSystem`은 디렉터리 정션으로 붙어 있고 이 저장소에서는 무시된다. 클론한
뒤에는 따로 받아서 `Plugins/`에 넣어야 한다.

## 실행

에디터에서 **Play 드롭다운 → VR Preview**. 패키지 빌드는 `bStartInVR=True`라 헤드셋이
붙어 있으면 스테레오로, 없으면 평면으로 뜬다.

리센터는 차량 리셋(`R`)에 딸려 있다. `ResetVehicle`이 끝에 `DoRecenterVR`을 부르므로 시야는
돌아오지만, 차가 들려 올라가고 속도가 0이 된다. 주행 중에 시야만 바로잡을 방법은 아직 없다.
전용 바인딩은 폰에 있으나 `IA_Reset_VR`을 가리키고 있고, 그 액션이 든 `IMC_Vehicle_VR`은
등록되지 않으므로 닿지 않는다.

## 차량을 바꾸면 반드시 재는 것

프로파일(`DA_Driver_*`)의 숫자 중 셋은 난이도가 아니라 **차량 특성**이다. 차를 바꾸면
그대로 다른 차의 값이 된다.

| 값 | 뜻 | 안 맞으면 |
|---|---|---|
| `MinTurnRadius` | 주행 속도대에서 풀락으로 그린 원의 반지름 | 크면 조향 게인이 모자라 코너 바깥으로 밀려 벽을 긁는다 |
| `LateralAccelBudget` | 그 차가 낼 수 있는 횡가속의 몇 % | 낮으면 코너에서 기어가고, 높으면 물고 들어가다 밀려난다 |
| `BrakingDecel` | 실측 제동 감속 | 낮으면 일찍 제동해 느리고, 높으면 코너 진입에서 넘친다 |

재는 방법은 콘솔 두 줄이다. 조작 방식과 무관하므로 스티어링 휠로 재도 된다.

```
racing.Calibrate 1      -- 시작
                        -- 직선 풀 스로틀 (최고 속도, 0-100)
                        -- 달리다 풀 브레이크 (제동 감속)
                        -- 넓은 곳에서 풀락으로 몇 바퀴 (횡가속, 유효 반경)
racing.Calibrate 0      -- 결과와 세 프로파일 권장값 출력
```

**측정 전에 에디터를 반드시 앞으로 꺼낼 것.** 백그라운드에서는 3fps로 돌아 값이 전부
무의미해진다. PIE를 스크립트로 폴링하면서 재도 안 된다. 폴링이 프레임 시간을 부풀린다.

측정된 템플릿 스포츠카(2026-09-10, `BP_SportsCar_Pawn`):
질량 1500 kg, 최대 토크 750 Nm에 곡선 최고 0.971(3000 rpm에서 728 Nm),
6000 rpm에서 600 Nm이므로 약 505 hp, 5단에 최종 감속 2.81, 항력 계수 0.31.
참고로 Corvette C8 Stingray는 1530 kg, 637 Nm, 495 hp, 0-100 약 2.9초, 최고 312 km/h다.
출력 대 질량은 이미 비슷하므로, 차가 느리게 느껴진다면 토크보다 **기어비, 타이어 마찰
(`FrictionForceMultiplier`), 변속 시점(`ChangeUpRPM`)** 쪽을 먼저 본다.

## VR에서 주의할 것

아래는 전부 이 프로젝트에서 실제로 부딪혀 확인한 것들이다.

**가상 텍스처를 켜면 instanced stereo가 크래시한다.** `vr.InstancedStereo=True`와
`r.VirtualTextures=True`가 함께 있으면 정점 셰이더가 `SV_ViewID`를 픽셀 셰이더로 넘기지
못해 PSO 생성이 실패한다. 레이트레이싱, 셰이더 캐시, Substrate, MetaXR, 포워드 셰이딩을
차례로 배제해도 재현 해시가 동일했고, 크래시 흔적이 `VirtualTextureFinalizeRequests`를
가리켰다. 지금은 `r.VirtualTextures=False`다. 두 맵 모두 런타임 가상 텍스처를 쓰지 않으므로
잃는 것이 없다.

**VR 카메라의 상대 위치는 카메라 자신에 두면 안 된다.** `UCameraComponent::HandleXRCamera`가
`bLockToHmd` 카메라의 상대 트랜스폼을 매 프레임 덮어쓴다. 눈높이 오프셋은 부모
씬 컴포넌트(`VROrigin`)에 둔다.

**리센터는 BeginPlay에서 하면 조용히 버려진다.** 그 시점에는 헤드셋 포즈가 아직 없다.
`IsTracking(HMDDeviceId)`가 참이 될 때까지 Tick에서 재시도한다.

**차량마다 손이 생기지 않게 한다.** 핸드 트래킹은 세상에 한 사람의 손만 보고하는데, 모든
차가 같은 폰 클래스를 쓰면 차마다 그 손을 자기 핸들과 비교하고 "운전자가 손을 뗐다"고
판정한다. 차가 넷이면 손이 여덟 개가 된다. 지금은 첫 틱에 운전석 여부를 보고 아니면 손
컴포넌트를 파괴한다. 감추는 것만으로는 부족하다. 트래킹 손 컴포넌트는 보이든 안 보이든
매 프레임 런타임을 조회한다.

**MetaXR는 `IHandTracker`를 등록하지 않는다.** 엔진의 핸드 트래킹 모듈러 피처로 손을 찾는
코드는 MetaXR만 켜서는 아무것도 얻지 못한다. `vr.HandProbe`로 엔진 경로와 MetaXR 경로를
나란히 확인할 수 있게 해 뒀다.

**뷰포트에 올린 UI는 헤드셋으로 가고 모니터로는 가지 않는다.** 이름과 코드를 읽고 짐작한 것과
정반대다. Slate가 그려지는 대상 텍스처의 이름이 `StereoSpectatorSwapChainTexture`라 관객
화면으로만 갈 것처럼 보이지만, 실제로 재보면 `AddToViewport`한 위젯은 운전자 눈앞에 뜨고
모니터에는 나오지 않는다.

그래서 그냥 두면 양쪽 다 반대가 된다. 운전자는 계기가 얼굴에 붙어 있고 관객은 아무것도 못 본다.

    vr.SpectatorProbe   빨간 배너를 AddToViewport로  →  헤드셋에만 보인다
    vr.SpectatorUI      초록 배너를 렌더타겟으로     →  모니터에만 보인다

관객에게 무언가 보여주려면 `UVRSpectatorUISubsystem::ShowSpectatorWidget`을 쓴다. 위젯을
렌더타겟에 그려 헤드셋의 스펙테이터 화면에 눈 이미지 위 오버레이로 넘기므로, 헤드셋에
전달되는 장면에는 들어가지 않는다.

**소리는 실내와 실외가 다르다.** VehicleSoundSystem README의 "VR에서 쓸 때"를 참고.

## 성능 (RTX 3060 12GB 실측, 2026-09-07)

Quest 2, 눈당 2080×2096(합계 8.7 MPix) 기준. 플랫 PIE에서 같은 픽셀 수로 재현한 값이다.

| 픽셀 | 최저 품질 | Epic |
|---|---|---|
| 8.4 MPix (현재 VR 해상도) | 26.0 ms | 49.0 ms |
| 4.2 MPix | 16.1 ms | 23.3 ms |
| 2.1 MPix | 10.5 ms | 15.1 ms |

**이 GPU로는 현재 해상도에서 90Hz(11.1ms)가 불가능하다.** 거의 빈 맵을 최저 품질로 그려도
26ms다. 설정으로 해결되는 문제가 아니다.

읽어낸 것 둘:

- **해상도를 먼저 깎고 품질은 나중에 깎는다.** Epic 품질 + 절반 해상도(23.3ms)가 최저 품질 +
  전체 해상도(26.0ms)보다 빠르다. 화질이 중요하다면 순서가 반대여야 한다
- **가장 비싼 단일 항목은 SkyAtmosphere다.** Epic에서 12.1ms. Lumen GI는 7.9ms, 가상
  그림자맵은 1.4ms, Lumen 반사는 0ms였다. 하늘 픽셀마다 도는 레이마칭이 Lumen보다 비싸다

데모를 다른 기계에서 돌린다면 다시 재야 한다. 위 숫자는 전부 RTX 3060 기준이다.

## 개발 중 알아둘 것

**`bSubsteppingAsync=True`라 `OnActorHit`이 게임 스레드로 오지 않는다.** 충돌 이벤트에
의존하는 코드는 동작하지 않는다. 충돌음은 속도 변화로 대신 잡는다.

**CSV 프로파일러를 쓸 수 없다.** 5.7.4에서 `-csvGpuStats`나 `-csvCaptureFrames`를 주면
시작 직후 `IsRayTracingAllowed() may only be called once RHI is initialized`로 죽는다.
순정 `-game`은 정상이다. 성능은 프레임 카운트 델타나 `stat unit`으로 잰다.

**출발 전 잠금은 폰 입력만 끄는 것으로는 새어 나간다.** `APawn::DisableInput`은
`bInputEnabled`를 내릴 뿐이고, `APlayerController::BuildInputStack`에서 빠지는 것은 그
폰의 입력 컴포넌트 하나다. 컨트롤러 자신의 입력 컴포넌트는 그대로 남으므로, 스티어링 휠처럼
플레이어 컨트롤러 쪽에 바인딩된 조작은 잠금을 통과한다. 그래서 `URaceParticipantComponent`는
잠긴 동안 매 틱 차량을 직접 붙잡는다(속도 0, 밀리면 제자리로). 입력 경로와 무관하게 같은
결과가 나온다.

**에디터가 백그라운드면 렌더를 건너뛴다.** 스크린샷이 픽셀 단위로 같거나 프로파일이 Slate만
잡히면 이것이다. 측정 전에 창을 앞으로 꺼내야 하고, 다른 프로세스에서 `SetForegroundWindow`를
부르면 거부되므로 `AttachThreadInput`으로 먼저 입력 상태를 붙여야 한다.

## 남은 일

- **카운트다운 사운드 연결.** `Content/RaceSystem/Audio/Race/`에 파일은 있으나 아무것도
  재생하지 않는다. `RaceDirectorSubsystem`의 `OnCountdownTick`(3,2,1,0 네 번 발사되므로
  0은 걸러야 한다)과 `OnRaceStarted`에 `BP_VehicleAdvGameMode`에서 물린다. 카운트다운은
  머리에 붙어야 하므로 Play Sound **2D**로 재생한다
- **성능.** 위 표 참고. 목표 기계가 정해지면 다시 측정한다
- **멀미 완화.** 미착수. 차체 롤 감쇠와 리센터 페이드가 후보
- **주행 중 시야만 바로잡을 수단.** 지금은 차량 리셋(`R`)에 딸려 있어 차가 멈춘다. 전용
  `IA_RecenterVR`을 만들어 `IMC_Vehicle_Default`에 매핑하고 폰의 `RecenterVRAction`에
  물리면 된다. C++ 쪽은 준비되어 있다
- **충돌 스파크의 나이아가라 시스템.** `UVehicleImpactFXComponent`는 폰에 붙어 있고 접촉
  지점까지 찾아내지만, `SparkSystem`이 비어 있어 아무것도 나오지 않는다. 스파크 시스템을
  하나 만들어 지정하면 된다. `Severity` 부동소수 파라미터를 두면 세게 박을수록 많이 튄다
- **도색 대상 지정.** `URacingLiveryApplierComponent`를 차량에 붙이고 `PaintMaterials`에
  차체 도장 머티리얼을 넣으면, 슬롯 이름과 번호가 메시마다 달라도 전부 잡힌다. 구성은
  `racing.DumpMaterialSlots`로 먼저 확인한다
- **노면별 타이어 소리.** `SurfaceTypeMapping`이 비어 있어 어디를 달려도 아스팔트로 나간다
- **실제 트랙으로 옮길 때 `TrackHalfWidth`.** 트랙 전체에 상수 하나라, 서킷의 좁은 구간에
  맞추면 넓은 곳에서 도로를 다 쓰지 않고 넓은 곳에 맞추면 좁은 구간에서 밖으로 나간다.
  구간별 폭이 필요해지면 그때 넣는다

## 다루지 않기로 한 것

- **운전자용 HUD.** 이번 데모에서는 운전자에게 계기를 보여주지 않기로 했다. VR에서 읽을 수
  있는 것은 월드 스페이스 위젯뿐이므로, 필요해지면 대시보드에 붙인다. 스크린 스페이스 위젯은
  VR에서 뷰포트에 올리지 않는다 - 올리면 운전자 얼굴에 붙는다
- **관객 화면 UI의 내용.** 띄우는 경로는 만들어 뒀다(`UVRSpectatorUISubsystem`). 무엇을
  띄울지는 정해지지 않았다. 순위와 랩이 후보고, `RaceDirectorSubsystem`에 필요한 델리게이트가
  모두 나와 있다
- **스크레이프 사운드.** 비동기 물리에서는 접촉 정보가 오지 않는다. `UVehicleImpactFXComponent`가
  쓰는 방법(속도 변화로 충돌을 잡고, 밀린 반대 방향으로 스윕해 접촉면을 찾는다)이면 지점은
  구할 수 있으므로 되살릴 여지는 있다. 지금은 소리 쪽에 붙이지 않았다
- **차량 템플릿 변형.** `Variant_TimeTrial`, `OffroadCar`, `Lvl_Offroad`, `Lvl_Timetrial`은
  데모 두 맵 어디서도 참조하지 않는 템플릿 잔재다. 런타임 비용이 없어 남겨 둔다
