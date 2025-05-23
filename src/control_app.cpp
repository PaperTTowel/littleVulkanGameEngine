#include "control_app.hpp"

#include "lve_buffer.hpp"
#include "lve_camera.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include "keyboard_movement_controller.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <array>
#include <chrono>
#include <cassert>
#include <iostream>
#include <stdexcept>

namespace lve {

  ControlApp::ControlApp() {
    globalPool = LveDescriptorPool::Builder(lveDevice)
      .setMaxSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT)
      .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, LveSwapChain::MAX_FRAMES_IN_FLIGHT)
      .build();

    // build frame descriptor pools
    framePools.resize(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    auto framePoolBuilder = LveDescriptorPool::Builder(lveDevice)
      .setMaxSets(1000)
      .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000)
      .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
      .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);
    for (int i = 0; i < framePools.size(); i++) {
      framePools[i] = framePoolBuilder.build();
    }
    loadGameObjects();
  }

  ControlApp::~ControlApp() {}

  void ControlApp::run() {

    std::vector<std::unique_ptr<LveBuffer>> uboBuffers(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++) {
      uboBuffers[i] = std::make_unique<LveBuffer>(
        lveDevice,
        sizeof(GlobalUbo),
        1,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      uboBuffers[i]->map();
    }

    auto globalSetLayout = LveDescriptorSetLayout::Builder(lveDevice)
      .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
      .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(LveSwapChain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
      auto bufferInfo = uboBuffers[i]->descriptorInfo();
      LveDescriptorWriter(*globalSetLayout, *globalPool)
        .writeBuffer(0, &bufferInfo)
        .build(globalDescriptorSets[i]);
    }

    std::cout << "Alignment: " << lveDevice.properties.limits.minUniformBufferOffsetAlignment << "\n";
    std::cout << "atom size: " << lveDevice.properties.limits.nonCoherentAtomSize << "\n";

    SimpleRenderSystem simpleRenderSystem {
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
    PointLightSystem pointLightSystem {
      lveDevice,
      lveRenderer.getSwapChainRenderPass(),
      globalSetLayout->getDescriptorSetLayout()};
    LveCamera camera{};

    auto &viewerObject = gameObjectManager.createGameObject();
    viewerObject.transform.translation.z = -2.5f;
    // 개발자모드 전용 (자유로운 카메라 움직임)
    // KeyboardMovementController cameraController{};
    CharacterMovementController characterController{};
    // 캐릭터랑 카메라의 오프셋 위치 (카메라 초기값)
    viewerObject.transform.rotation.y += 1.575f;
    viewerObject.transform.rotation.x -= 0.4f;

    auto currentTime = std::chrono::high_resolution_clock::now();

    while (!lveWindow.shouldClose()) {
      glfwPollEvents();

      auto newTime = std::chrono::high_resolution_clock::now();
      float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
      currentTime = newTime;
      
      // 개발자모드 전용 (자유로운 카메라 움직임)
      // cameraController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, viewerObject);
      camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

      float aspect = lveRenderer.getAspectRatio();
      camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);

      // 캐릭터 업데이트
      auto &character = gameObjectManager.gameObjects.at(characterId);
      characterController.moveInPlaneXZ(lveWindow.getGLFWwindow(), frameTime, character);
      gameObjectManager.updateFrame(character, 6, frameTime, 0.15);

      // 카메라가 부드럽게 캐릭터를 따라가도록 지정
      glm::vec3 targetPos = character.transform.translation + glm::vec3(-3.0f, -2.0f, .0f);
      viewerObject.transform.translation = glm::mix(viewerObject.transform.translation, targetPos, 0.1f);

      if (auto commandBuffer = lveRenderer.beginFrame()) {
        int frameIndex = lveRenderer.getFrameindex();
        framePools[frameIndex]->resetPool();
        FrameInfo frameInfo{
          frameIndex,
          frameTime,
          commandBuffer,
          camera,
          globalDescriptorSets[frameIndex],
          *framePools[frameIndex],
          gameObjectManager.gameObjects};

        // update
        GlobalUbo ubo{};
        ubo.projection = camera.getProjection();
        ubo.view = camera.getView();
        ubo.inverseView = camera.getInverseView();
        pointLightSystem.update(frameInfo, ubo);
        uboBuffers[frameIndex]->writeToBuffer(&ubo);
        uboBuffers[frameIndex]->flush();

        // update physics engine
        std::vector<LveGameObject*> objPtrs;
        objPtrs.reserve(gameObjectManager.gameObjects.size());
        for (auto& [id, obj] : gameObjectManager.gameObjects) {
            objPtrs.push_back(&obj);
        }
        gameObjectManager.physicsEngine->stepSimulation(frameIndex);
        gameObjectManager.physicsEngine->syncTransforms(objPtrs);

        // final step of update is updating the game objects buffer data
        // The render functions MUST not change a game objects transform data
        gameObjectManager.updateBuffer(frameIndex);

        // render
        lveRenderer.beginSwapChainRenderPass(commandBuffer);

        // order here matters
        simpleRenderSystem.renderGameObjects(frameInfo);
        pointLightSystem.render(frameInfo);

        lveRenderer.endSwapChainRenderPass(commandBuffer);
        lveRenderer.endFrame();
      }
    }

    vkDeviceWaitIdle(lveDevice.device());
  }

  void ControlApp::loadGameObjects() {

    std::shared_ptr<LveModel> lveModel = LveModel::createModelFromFile(lveDevice, "models/testMap.obj");

    auto &gameObj = gameObjectManager.createGameObject();
    gameObj.model = lveModel;
    gameObj.enableTextureType = 0;
    gameObj.transform.translation = {-.5f, .5f, 0.f};
    gameObj.transform.scale = glm::vec3(3.f);

    gameObjectManager.physicsEngine->addBoxRigidBody(gameObj, .0f);

    lveModel = LveModel::createModelFromFile(lveDevice, "models/character.obj");
    std::shared_ptr<LveTexture> marbleTexture =
      LveTexture::createTextureFromFile(lveDevice, "../textures/cp.png");

    auto &characterObj = gameObjectManager.createGameObject();
    characterObj.model = lveModel;
    characterObj.enableTextureType = 1;
    characterObj.diffuseMap = marbleTexture;
    characterObj.transform.translation = {-.5f, .5f, 0.f};
    characterObj.transform.scale = glm::vec3(3.f);
    characterId = characterObj.getId();

    std::vector<glm::vec3> lightColors{
        {1.f, .1f, .1f},
        {.1f, .1f, 1.f},
        {.1f, 1.f, .1f},
        {1.f, 1.f, .1f},
        {.1f, 1.f, 1.f},
        {1.f, 1.f, 1.f}};

    for (int i = 0; i < lightColors.size(); i++) {
      auto &pointLight = gameObjectManager.makePointLight(0.2f);
      pointLight.color = lightColors[i];
      auto rotateLight = glm::rotate(
        glm::mat4(1.f),
        (i * glm::two_pi<float>()) / lightColors.size(),
        {0.f, -1.f, 0.f});
      pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, -1.f));
    }
  }
} // namespace lve