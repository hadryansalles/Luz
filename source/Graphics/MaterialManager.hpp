#pragma once

#include "Model.hpp"
#include "Material.hpp"

class MaterialManager {
private:
    static void DiffuseOnImgui(Model* model);
    static void SpecularOnImgui(Model* model);
public:
    static void OnImgui(Model* model);

    static inline std::string GetTypeName(MaterialType type) {
        switch (type) {
        case MaterialType::Unlit:
            return "Unlit";
        case MaterialType::Phong:
            return "Phong";
        default:
            return "Invalid";
        }
    }
};

// #pragma once

// #include "Model.hpp"
// #include "Material.hpp"

// class MaterialManager {
//     static inline const char* TYPE_NAMES[] = {"Phong", "Unlit"};

//     static inline constexpr int MAX_PHONG_MATERIALS = 2048;
//     static inline constexpr int MAX_UNLIT_MATERIALS = 2048;
//     static inline PhongMaterial phongData[MAX_PHONG_MATERIALS];
//     static inline UnlitMaterial unlitData[MAX_UNLIT_MATERIALS];

//     static inline std::vector<Material*> materials;

//     static void DiffuseOnImgui(Model* model);
//     static void SpecularOnImgui(Model* model);
// public:
//     static void OnImgui(Model* model);
// };
