#include "transform.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>
#include <xxhash.h>

namespace strings {
uint64_t toHash ( const std::string& string, bool addBOM ) {
	if ( !addBOM ) {
		return XXH3_64bits ( string.data (), string.size () );
	}
	std::string data;
	data.reserve ( string.size () + 3 );
	data.append ( "\xEF\xBB\xBF", 3 );
	data += string;
	return XXH3_64bits ( data.data (), data.size () );
}

void trim ( std::string& string ) {
	if ( string.size () >= 3 &&
			 static_cast<unsigned char> ( string [ 0 ] ) == 0xEF &&
			 static_cast<unsigned char> ( string [ 1 ] ) == 0xBB &&
			 static_cast<unsigned char> ( string [ 2 ] ) == 0xBF ) {
		string.erase ( 0, 3 );
	}
	const auto begin = string.begin ();
	string.erase (
			begin, std::find_if_not ( begin, string.end (), [] ( unsigned char c ) {
				return isspace ( c );
			} ) );
	string.erase (
			std::find_if_not ( string.rbegin (), string.rend (),
												 [] ( unsigned char c ) { return isspace ( c ); } )
					.base (),
			string.end () );
}

std::string trim ( std::string&& string ) {
	trim ( string );
	return std::move ( string );
}

std::vector<std::string> split ( const std::string& string,
																 const std::string& splitter ) {
	std::vector<std::string> result;
	size_t start = 0, end = 0, len = splitter.length ();
	auto add = [ & ] {
		auto chunk = trim ( string.substr ( start, end - start ) );
		if ( !chunk.empty () ) {
			result.emplace_back ( chunk );
		}
	};
	while ( ( end = string.find ( splitter, start ) ) != std::string::npos ) {
		add ();
		start = end + len;
	}
	add ();
	return result;
}

std::string lower ( const std::string& str ) {
	std::string result;
	result.resize ( str.size () );
	std::transform ( str.begin (), str.end (), result.begin (),
									 [] ( unsigned char c ) { return std::tolower ( c ); } );
	return result;
}
}
