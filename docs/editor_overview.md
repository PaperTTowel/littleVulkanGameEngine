# Editor 개요

이 문서는 에디터 모듈 구성과 역할을 요약한다.

## 디렉터리 구성

- `src/Editor/History`  
  히스토리/Undo-Redo 관련 기능.
- `src/Editor/Tools`  
  편집 도구(선택, 이동, 배치 등).
- `src/Editor/UI`  
  패널, 위젯, 공용 UI 컴포넌트.
- `src/Editor/Workflow`  
  워크플로/불러오기/내보내기 흐름.
- `src/Editor/editor_system.cpp`  
  에디터 전체 초기화 및 업데이트 오케스트레이션.

## 의존 방향

- 에디터는 `Engine`을 사용한다.
- 에디터는 `Backend`에 직접 의존하지 않는다.
- 렌더링 미리보기는 엔진/백엔드 인터페이스를 통해 접근한다.

## 설계 원칙

- UI/Tool/Workflow는 서로 느슨하게 연결한다.
- 히스토리는 모든 편집 작업의 공통 경로로 유지한다.
- 에디터 전용 로직은 엔진 코어와 분리한다.

## 확장 포인트

- 새로운 Tool 추가: `Tools`에 추가 + `editor_system` 등록
- 새로운 패널 추가: `UI`에 패널 추가
- 새로운 워크플로 추가: `Workflow`에 단계 정의

