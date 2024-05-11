#include "scene_panel.hpp"

#include "log.hpp"
#include "scene.hpp"
#include "math/math.hpp"

#include "component/transform_component.hpp"

#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

ScenePanel::ScenePanel() : _frameBuffer(FrameBuffer()),
                           _pickingBuffer(FrameBuffer()), _camera(Camera()),
                           _modelShader(Shader("shaders/model.vert", "shaders/model.frag")),
                           _lightShader(Shader("shaders/light.vert", "shaders/light.frag")),
                           _outlineShader(Shader("shaders/model.vert", "shaders/outline.frag")),
                           _pickingShader(Shader("shaders/picking.vert", "shaders/picking.frag"))
{
}

void ScenePanel::Render(GLFWwindow *window)
{
    ImGui::ShowDemoWindow();

    RenderPass();

    ImGui::Begin("Scene");

    auto panelSize = ImGui::GetContentRegionAvail();
    if (panelSize.x != _width || panelSize.y != _height)
        Resize(panelSize.x, panelSize.y);

    uint64_t textureId = _frameBuffer.GetTextureId();
    ImGui::Image(reinterpret_cast<void *>(textureId), ImVec2{_width, _height}, ImVec2{0, 1}, ImVec2{1, 0});

    _camera.Input(panelSize.x, panelSize.y, window, ImGui::IsWindowHovered());
    _camera.Update(panelSize.x, panelSize.y);

    HandleGizmo(window);

    ImGui::End();
}

void ScenePanel::RenderPass()
{
    _frameBuffer.Bind();

    glClearColor(0.31f, 0.41f, 0.46f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // for (auto &gameObject : Scene::GameObjects)
    // {
    // }

    _frameBuffer.Unbind();
}

void ScenePanel::Input(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        Scene::ActiveScene->SelectedEntity.reset();

    auto isUsingMouse = ImGuizmo::IsUsing() || _camera.IsMouseLocked();

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && !isUsingMouse)
        _activeGizmo = ImGuizmo::OPERATION::TRANSLATE;

    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && !isUsingMouse)
        _activeGizmo = ImGuizmo::OPERATION::ROTATE;

    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !isUsingMouse)
        _activeGizmo = ImGuizmo::OPERATION::SCALE;
}

void ScenePanel::Resize(float width, float height)
{
    _width = width;
    _height = height;
    _frameBuffer.CreateBuffer(width, height);
    _pickingBuffer.CreateBuffer(width, height);
}

void ScenePanel::OnMouseClick()
{
    return;

    // if (Scene::SelectedGameObject && (ImGuizmo::IsUsing() || ImGuizmo::IsOver()))
    //     return;

    _pickingBuffer.Bind();

    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // for (int i = 0; i < Scene::GameObjects.size(); ++i)
    // {
    //     auto &gameObject = Game::GameObjects[i];
    //     _pickingShader.Bind();
    //     _pickingShader.SetVec3(Shader::PICKING_COLOR, _pickingBuffer.EncodeId(i));
    //     gameObject.Render(_pickingShader);
    // }

    auto [mouseX, mouseY] = ImGui::GetMousePos();
    mouseX -= _viewPortBounds[0].x;
    mouseY -= _viewPortBounds[0].y;
    glm::vec2 viewportSize = _viewPortBounds[1] - _viewPortBounds[0];
    mouseY = viewportSize.y - mouseY;

    // if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)viewportSize.x && mouseY < (int)viewportSize.y)
    // {
    //     auto index = _pickingBuffer.DecodePixel(mouseX, mouseY);
    //     Scene::SelectedGameObject = index != -1 ? &Scene::GameObjects[index] : nullptr;
    // }

    _pickingBuffer.Unbind();
}

void ScenePanel::HandleGizmo(GLFWwindow *window)
{
    auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
    auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
    auto viewportOffset = ImGui::GetWindowPos();
    _viewPortBounds[0] = {viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y};
    _viewPortBounds[1] = {viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y};

    if (!Scene::ActiveScene->SelectedEntity.has_value())
        return;

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(_viewPortBounds[0].x, _viewPortBounds[0].y, _viewPortBounds[1].x - _viewPortBounds[0].x, _viewPortBounds[1].y - _viewPortBounds[0].y);

    auto shouldSnap = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
    float snapValue = 0.5f; // Snap to 0.5m for translation/scale
    if (_activeGizmo == ImGuizmo::OPERATION::ROTATE)
        snapValue = 45.0f;

    float snapValues[3] = {snapValue, snapValue, snapValue};

    auto transformComponent = Scene::ActiveScene->Registry.try_get<TransformComponent>(Scene::ActiveScene->SelectedEntity.value());
    if (!transformComponent)
        return;

    auto transform = transformComponent->GetTransform();
    ImGuizmo::Manipulate(glm::value_ptr(_camera.GetViewMatrix()),
                         glm::value_ptr(_camera.GetProjectionMatrix()),
                         _activeGizmo, ImGuizmo::WORLD,
                         glm::value_ptr(transform),
                         nullptr,
                         shouldSnap ? snapValues : nullptr,
                         nullptr,
                         nullptr);

    if (ImGuizmo::IsUsing())
    {
        glm::vec3 position, rotation, scale;
        DecomposeTransform(transform, position, rotation, scale);

        glm::vec3 deltaRotation = rotation - transformComponent->Rotation;
        transformComponent->Position = position;
        transformComponent->Rotation += deltaRotation;
        transformComponent->Scale = scale;
    }
}
