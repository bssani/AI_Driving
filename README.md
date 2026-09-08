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
- **노면별 타이어 소리.** `SurfaceTypeMapping`이 비어 있어 어디를 달려도 아스팔트로 나간다
- **실제 트랙으로 옮길 때 `TrackHalfWidth`.** 트랙 전체에 상수 하나라, 서킷의 좁은 구간에
  맞추면 넓은 곳에서 도로를 다 쓰지 않고 넓은 곳에 맞추면 좁은 구간에서 밖으로 나간다.
  구간별 폭이 필요해지면 그때 넣는다

## 다루지 않기로 한 것

- **HUD.** 속도계와 기어를 포함해 모든 위젯이 `AddToViewport`, 즉 스크린 스페이스다. VR에서는
  제대로 보이지 않지만 이번 데모에서는 필요하지 않다고 판단했다. 필요해지면 월드 스페이스
  위젯을 대시보드에 붙인다
- **스크레이프 사운드.** 비동기 물리에서는 접촉 정보가 오지 않아 살릴 수 없다
- **차량 템플릿 변형.** `Variant_TimeTrial`, `OffroadCar`, `Lvl_Offroad`, `Lvl_Timetrial`은
  데모 두 맵 어디서도 참조하지 않는 템플릿 잔재다. 런타임 비용이 없어 남겨 둔다
