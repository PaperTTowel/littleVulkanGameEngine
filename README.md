# 게임엔진 프로젝트
게임엔진 개발 프로젝트

사용한 Vulkan 기반 소스코드는 [이 링크](https://github.com/blurrypiano/littleVulkanEngine)를 통해 확인하실 수 있습니다.

## 빌드환경
**참고용**
+ Win
  - MSVC 컴파일러 이용
  - VulkanSDK 1.4.328.1

---

+ Fedora


    다음 명령어를 통해 필요한 저장소를 설치하십시오.
  + 개발 도구 키트 (gcc 미포함)
  ```
  dnf install @development-tools
  ```

  + Vulkan API (셰이더 컴파일 추가)
  ```
  dnf install vulkan-devel vulkan-validation-layers
  ```
  
## Credits
유튜브 튜토리얼을 제작한 [Brendan Galea](https://www.youtube.com/watch?v=Y9U9IE0gVHA&list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR)에게 감사드립니다!

vulkan 공부에 많은 도움이 되었던 [이 가이드집](https://vkguide.dev/docs/ko)을 제작한 분들에게 감사드립니다!
