#pragma once

#include "ProjectDocument.h"

#include <filesystem>
#include <string>

class ProjectRepository {
public:
    static bool Save(const std::filesystem::path& path, const ProjectDocument& document, std::wstring& error);
    static bool Load(const std::filesystem::path& path, ProjectDocument& document, std::wstring& error);
};
