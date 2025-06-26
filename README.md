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


## 개발환경
리포지토리를 뜯어갈때 참고용으로 사용하시길 바랍니다.
+ Windows 11
  + 공통
    - VulkanSDK 1.4.313.1
    - glm 1.0.1-1
    - glfw 3.4
  + PaperTTowel의 개발 환경 (**참고용**)
    - Windows 11 with PowerShell 7
    - cmake version 4.0.0-rc2
    - mingw-w64-ucrt-x86_64-gcc 14.2.0-2
    - mingw-w64-ucrt-x86_64-glm 1.0.1-1
    - mingw-w64-ucrt-x86_64-glfw 3.4-1
    - mingw-w64-ucrt-x86_64-bullet 3.25-7

 **Windows 환경에서 msys2Build.bat를 사용하실려면 아래 UCRT64 패키지를 참고해주세요!**
 + Msys2 UCRT64 사용한 패키지 버전
   - mingw-w64-ucrt-x86_64-cmake 4.0.3-1
   - make 4.4.1-2
---

+ Fedora 42 KDE


    다음 명령어를 통해 필요한 저장소를 설치하십시오.
  + 개발 도구 키트 (gcc 미포함)
  ```
  dnf install @development-tools
  ```
  + glm, glfw
  ```
  dnf install glm-devel glfw-devel
  ```

  + Vulkan API (셰이더 컴파일 추가)
  ```
  dnf install vulkan-devel vulkan-validation-layers
  ```
  + 물리엔진
  ```
  dnf install bullet-devel
  ```
  
## Credits
유튜브 튜토리얼을 제작한 [Brendan Galea](https://www.youtube.com/watch?v=Y9U9IE0gVHA&list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR)에게 감사드립니다!

vulkan 공부에 많은 도움이 되었던 [이 가이드집](https://vkguide.dev/docs/ko)을 제작한 분들에게 감사드립니다!
