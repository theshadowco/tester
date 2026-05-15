#pragma once
#include <filesystem>
#include <string>

namespace tooltips_designer {
std::string get ( const std::filesystem::path& sources,
									const std::string& formName,
									const std::string& language );
}
