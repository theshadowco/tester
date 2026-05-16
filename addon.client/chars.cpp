#include "chars.h"
#include <algorithm>
#include <codecvt>
#include <cstring>
#include <locale>
#include <stdexcept>
#include <string>
#include "corestrings.h"

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays)
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)

namespace {
std::u16string utf8ToUtf16 ( const std::string& source ) {
	std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
	return converter.from_bytes ( source );
}

std::string utf16ToUtf8 ( const std::u16string& source ) {
	std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
	return converter.to_bytes ( source );
}
}

uint32_t Chars::WCHARLength ( const WCHAR_T *Source ) {
	uint32_t length = 0;
	auto tmpShort = const_cast<WCHAR_T *>( Source );
	while ( *tmpShort++ ) ++length;
	return length;
}

std::unique_ptr<wchar_t []> Chars::FromWCHAR ( const WCHAR_T *Source, size_t Length ) {
	if ( !Length ) {
		Length = WCHARLength ( Source );
	}
	++Length;
	std::unique_ptr<wchar_t []> result { new wchar_t [ Length ] };
	auto receiver = result.get ();
	memset ( receiver, 0, Length * sizeof ( wchar_t ) );
	while ( --Length && *Source ) {
		*receiver++ = *Source++;
	}
	return result;
}

std::wstring Chars::StringToWide ( const std::string &Source, bool cleanFirst ) {
	const auto input = cleanFirst ? strings::clean ( Source ) : Source;
	std::u16string temp;
	try {
		temp = utf8ToUtf16 ( input );
	} catch ( const std::range_error& ) {
		temp.assign ( input.begin (), input.end () );
	}
	std::wstring result;
	result.resize ( temp.size () );
	std::copy ( temp.begin (), temp.end (), result.begin () );
	return result;
}

std::string Chars::WideToString ( const std::wstring &Source ) {
	std::u16string temp;
	temp.resize ( Source.size () );
	std::copy ( Source.begin (), Source.end (), temp.begin () );
	try {
		return utf16ToUtf8 ( temp );
	} catch ( const std::range_error& ) {
		std::string result;
		result.reserve ( temp.size () );
		for ( auto character : temp ) {
			result.push_back ( static_cast<char> ( character ) );
		}
		return result;
	}
}

std::wstring Chars::WCHARToWide ( const WCHAR_T *String, size_t Length ) {
	if ( String == nullptr ) {
		return {};
	}
	return std::wstring { FromWCHAR ( String, Length ).get () };
}

// NOLINTEND(cppcoreguidelines-pro-type-const-cast)
// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
// NOLINTEND(cppcoreguidelines-avoid-c-arrays)
