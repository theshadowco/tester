#ifndef __sql_h__
#define __sql_h__
#include "./1c/types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct command {
	std::wstring name;
	short type;
	unsigned int index;
};

struct commandType {
	const wchar_t* signature;
	const short type;
	const unsigned int signatureLen;

	commandType ( const wchar_t* signature, short type )
			: signature ( signature ), type ( type ),
				signatureLen ( wcslen ( signature ) ) {
	}
};

class sql {
public:
	sql ( const wchar_t* query, const size_t length );
	std::wstring get ();
	std::wstring adjust ();

private:
	nlohmann::json json;
	const wchar_t* query;
	const wchar_t* row;
	const size_t length;
	const std::vector<commandType> commands {
			{ L"// #", 1 }, { L"// ^", 2 }, { L"// @", 3 } };
	size_t index;
	unsigned int len;

	void addRecord ();
	const wchar_t* begins ( const wchar_t* string1,
													const wchar_t* string2 ) const;
	bool nextQuery () const;
};
#endif
