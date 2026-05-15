#include "files.h"
#include "transform.h"
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <xxhash.h>

namespace files {
std::string toString ( const std::string& file ) {
	return toString ( std::filesystem::path ( file ) );
}

std::string toString ( const std::filesystem::path& file ) {
	std::ifstream stream ( file, std::ios::ate );
	if ( !stream ) {
		throw std::runtime_error ( "failed to open: " + file.string () );
	}
	std::string content;
	content.resize ( stream.tellg () );
	stream.seekg ( 0 );
	stream.read ( content.data (), content.size () );
	return content;
}

uint64_t toHash ( const std::string& file ) {
	return toHash ( std::filesystem::path ( file ) );
}

uint64_t toHash ( const std::filesystem::path& file ) {
	const auto string = strings::trim ( toString ( file ) );
	return XXH3_64bits ( string.data (), string.size () );
}
}
