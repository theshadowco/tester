#ifndef __appearanceparser_h__
#define __appearanceparser_h__
#include <memory_resource>
#include <string>
#include <unordered_set>
#include <vector>

namespace appearance {
struct Record {
	std::wstring directive;
	std::vector<std::wstring> controls;
	std::vector<std::wstring> appearance;
#ifdef __linux__
	std::unordered_set<std::wstring> fields;
#else
	std::pmr::unordered_set<std::wstring> fields;
#endif
	std::wstring expression;
	void clear () {
		controls.clear ();
		appearance.clear ();
		fields.clear ();
		expression.clear ();
		directive.clear ();
	}
};
enum class Contexts { controls, appearance, appearanceParams, expression };
enum class Symbols {
	letter,
	number,
	openingParenthesis,
	closingParenthesis,
	openingBracket,
	closingBracket,
	quote,
	slash,
	backslash,
	plus,
	minus,
	asterisk,
	percent,
	hash,
	question,
	colon,
	semicolon,
	dot,
	comma,
	greater,
	less,
	equal,
	bar,
	space,
	linebreak,
	undefined
};

namespace Errors {
const std::wstring expressionExpected { L"Expression is expected" };
const std::wstring controlExpected { L"Control name is expected" };
const std::wstring wrongCharacter { L"Wrong character" };
const std::wstring unknownAppearance { L"Unknown appearance" };
const std::wstring unknownAppearanceParam { L"Unknown appearance parameter" };
const std::wstring unknownDirective { L"Unknown directive" };
}
struct error {
	std::wstring message;
	size_t position;
};
class appearance {
	std::wstring rules;
	std::vector<Record> records;
	Record record;
	std::wstring id;
	std::wstring& expression { record.expression };
	Contexts context;
	Symbols character;
	decltype ( character ) lastCharacter;
	wchar_t item;
	decltype ( rules )::size_type position;
	decltype ( position ) startId;
	bool isString;

	void addRecord ();
	auto lower () const;
	void addControl ();
	bool addAppearance ();
	bool isId ();
	bool property () const;
	void addField ();
	void addCollection ();
	void harvestId ();
	void harvestExpression () const;
	void startExpression ();
	void init ( const std::wstring& rules );
	bool addAppearanceParams ();
	void adjustFunction ();
	bool addDirective ();
	void extract ();
	std::wstring convert ();

public:
	std::wstring parse ( const decltype ( rules )& rules );
};
}
#endif
