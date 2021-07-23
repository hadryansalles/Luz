#pragma once

#include <filesystem>

class Scene {
private:
    static inline std::filesystem::path path = "assets/Default.Luzscene";
    static inline bool dirty = true;
    static inline bool autoReloadFiles = false;

public:
    static void Init();
    static void Save();
    static void Destroy();
    static void OnImgui();
};
