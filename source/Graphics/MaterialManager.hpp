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
