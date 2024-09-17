#pragma once

struct CameraController {
    void Update(Ref<struct SceneAsset>& scene, Ref<struct CameraNode>& cam, bool viewportHovered);
};
