# 작은 Vulkan 게임엔진 프로젝트
Vulkan을 이용하여 나만의 작은 게임엔진을 개발하는 프로젝트입니다.

사용한 Vulkan 기반 소스코드는 [이 링크](https://github.com/blurrypiano/littleVulkanEngine)를 통해 확인하실 수 있습니다.

## 개발자 노트
*Vulkan 공부용으로 개발하고 있습니다!*

### NVIDIA RTX 4060 for Laptop기준으로 테스트됩니다.

> [main (stable)](https://github.com/PaperTTowel/littleVulkanGameEngine/tree/main)
> + 컴파일 에러가 발생하지 않는 안정적인 코드가 업로드 됩니다.


> [dev (preview)](https://github.com/PaperTTowel/littleVulkanGameEngine/tree/dev)
> + 앞으로 추가될 기능을 미리 볼 수 있으나, 많이 불안정할 수 있습니다.
> + 코드 최적화나, 셰이더 최적화 이슈로 렉이 발생할 수 있습니다.
> + 가끔 컴파일 에러가 발생하는 파일이 푸시될 수 있습니다.


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
