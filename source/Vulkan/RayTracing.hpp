#pragma once

struct Model;

namespace RayTracing {

void Create();
void Destroy();
void CreateBLAS(std::vector<RID>& meshes);
void CreateTLAS(const std::vector<Model*>& models);
void SetRecreateTLAS();
void OnImgui();

}