#pragma once
#include <cctype>
#include <cstdint>
#include <string>

namespace strings {
bool starts ( const std::wstring& source, std::wstring_view prefix );

bool ends ( const std::wstring& source, std::wstring_view suffix,
						bool dropSpaces = true );

std::wstring replace ( std::wstring string, const std::wstring& what,
											 const std::wstring& replace );

void replace ( std::wstring* string, const std::wstring& what,
							 const std::wstring& replace );

std::string clean ( const std::string& input );

std::wstring replaceAccents ( const std::wstring& input );

void replaceChars ( std::wstring& string, const std::wstring& list,
										const wchar_t replace );

inline bool isLetter ( const char symbol ) {
	return ( symbol >= 'A' && symbol <= 'Z' ) ||
				 ( symbol >= 'a' && symbol <= 'z' ) || symbol == '_';
}

inline bool isBlank ( const char symbol ) {
	return std::isspace ( symbol );
}

std::string getUnique ( int size );

template <typename T> std::wstring listToJson ( const T& list ) {
	std::wstring result { L"[" };
	for ( const auto& s : list ) {
		result += L"\"" + s + L"\",";
	}
	if ( result.length () > 1 ) {
		result.pop_back ();
	}
	return result + L"]";
}

template <typename Type>
inline bool contains ( const std::string& text, const Type& value ) {
	if constexpr ( std::convertible_to<Type, std::string_view> ) {
		return text.find ( std::string_view ( value ) ) != std::string::npos;
	} else {
		return text.find ( std::to_string ( value ) ) != std::string::npos;
	}
}

template <typename Type, typename... Args>
inline bool contains ( const std::string& text, const Type& value,
											 const Args&... args ) {
	return contains ( text, value ) || contains ( text, args... );
}

template <typename StringType>
inline std::string trim ( const StringType& string ) {
	if ( string.empty () ) {
		return {};
	}
	size_t start = 0, end = string.length () - 1;
	while ( start <= end && std::isspace ( string [ start ] ) )
		++start;
	while ( start < end && std::isspace ( string [ end ] ) )
		--end;
	return start > end ? std::string ()
										 : std::string ( string.substr ( start, end - start + 1 ) );
}
}
