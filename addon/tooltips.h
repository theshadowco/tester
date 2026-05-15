#pragma once
#include <string>
#include <filesystem>

class Tooltips {
public:
	Tooltips ( std::string sources, std::string formName, std::string language );
	std::string get ();

private:
	std::filesystem::path formPath () const;

	std::string sources, formName, language;
};
