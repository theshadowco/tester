#include "appearanceparser.h"
#include "corestrings.h"
#include <algorithm>
#include <clocale>
#include <unordered_set>
#ifdef __linux__
#include <cwctype>
#endif

namespace appearance {
const std::unordered_set<std::wstring> commands {
		L"show",		 L"показать", L"hide",			L"скрыть",		 L"enable",
		L"включить", L"disable",	L"выключить", L"lock",			 L"закрыть",
		L"unlock",	 L"открыть",	L"press",			L"прижать",		 L"release",
		L"отжать",	 L"mark",			L"отметить",	L"unmark",		 L"снятьотметку",
		L"title",		 L"назвать",	L"hint",			L"подсказать", L"assign",
		L"назначить"

};
const std::unordered_set<std::wstring> logic { L"not", L"не", L"and",
																							 L"и",	 L"or", L"или" };
const std::unordered_set<std::wstring> objects { L"catalog.",
																								 L"enum.",
																								 L"chartofcharacteristictypes.",
																								 L"chartofaccounts.",
																								 L"chartofcalculationtypes.",
																								 L"document.",
																								 L"businessprocess.",
																								 L"task.",
																								 L"exchangeplan.",
																								 L"справочник.",
																								 L"перечисление.",
																								 L"планвидовхарактеристик.",
																								 L"плансчетов.",
																								 L"планвидоврасчета.",
																								 L"документ.",
																								 L"бизнесспроцесс.",
																								 L"задача.",
																								 L"планобмена." };
const std::unordered_set<std::wstring> checkFill { L"filled", L"есть" };
const std::unordered_set<std::wstring> checkVoid { L"empty", L"пусто" };
const std::unordered_set<std::wstring> builtinFunctions { L"inlist", L"всписке",
																													L"field", L"поле" };
const std::unordered_set<std::wstring> directives { L"#c", L"#к", L"#s",
																										L"#с" };
const std::unordered_set<std::wstring> reserved {
		L"undefined", L"неопределено", L"null", L"true",
		L"истина",		L"false",				 L"ложь" };

const std::wstring builtinPrefix { L"Appearance." };
const std::wstring fieldsPrefix { L"Form." };
const std::wstring checkFillFunction { L"valueisfilled" };
const std::wstring checkVoidFunction { L"not " + checkFillFunction };
const std::wstring dataExtracter { L"PredefinedValue" };

bool isKeyword ( const std::wstring& string ) {
	return commands.find ( string ) != commands.end () ||
				 logic.find ( string ) != logic.end ();
}

bool isDirective ( const std::wstring& string ) {
	return strings::starts ( string, L"#" );
}

bool isAppearance ( const std::wstring& string ) {
	return commands.find ( string ) != commands.end ();
}

bool isReserved ( const std::wstring& string ) {
	return reserved.find ( string ) != reserved.end ();
}

bool isObject ( const std::wstring& string ) {
	for ( auto& item : objects ) {
		if ( strings::starts ( string, item.data () ) ) {
			return true;
		}
	}
	return false;
}

bool fillChecking ( const std::wstring& string ) {
	return checkFill.find ( string ) != checkFill.end ();
}

bool voidChecking ( const std::wstring& string ) {
	return checkVoid.find ( string ) != checkVoid.end ();
}

bool builtin ( const std::wstring& string ) {
	return builtinFunctions.find ( string ) != builtinFunctions.end ();
}

bool directive ( const std::wstring& string ) {
	return directives.find ( string ) != directives.end ();
}

Symbols getClass ( wchar_t symbol ) {
	if ( iswalpha ( symbol ) != 0 || symbol == '_' ) {
		return Symbols::letter;
	}
	if ( iswdigit ( symbol ) != 0 ) {
		return Symbols::number;
	}
	switch ( symbol ) {
	case '(':
		return Symbols::openingParenthesis;
	case ')':
		return Symbols::closingParenthesis;
	case '[':
		return Symbols::openingBracket;
	case ']':
		return Symbols::closingBracket;
	case '"':
		return Symbols::quote;
	case '/':
		return Symbols::slash;
	case '\\':
		return Symbols::backslash;
	case '+':
		return Symbols::plus;
	case '-':
		return Symbols::minus;
	case '*':
		return Symbols::asterisk;
	case '%':
		return Symbols::percent;
	case '#':
		return Symbols::hash;
	case '?':
		return Symbols::question;
	case ':':
		return Symbols::colon;
	case ';':
		return Symbols::semicolon;
	case '.':
		return Symbols::dot;
	case ',':
		return Symbols::comma;
	case '>':
		return Symbols::greater;
	case '<':
		return Symbols::less;
	case '=':
		return Symbols::equal;
	case '|':
		return Symbols::bar;
	case ' ':
		return Symbols::space;
	case '\t':
		return Symbols::space;
	case '\n':
		return Symbols::linebreak;
	default:
		return Symbols::undefined;
	}
}
void appearance::addRecord () {
	if ( expression.empty () ) {
		throw error { Errors::expressionExpected, position };
	}
	records.push_back ( record );
	record.clear ();
	id.clear ();
	context = Contexts::controls;
}

auto appearance::lower () const {
	auto result { id };
	std::transform ( result.begin (), result.end (), result.begin (),
									 ::towlower );
	return result;
}

void appearance::addControl () {
	if ( id.empty () || getClass ( id [ 0 ] ) == Symbols::number ) {
		throw error { Errors::controlExpected, position };
	}
	record.controls.push_back ( id );
	id.clear ();
}

bool appearance::addAppearance () {
	auto& format = record.appearance;
	auto keyword = lower ();
	if ( isAppearance ( keyword ) ) {
		format.push_back ( keyword );
	} else {
		return false;
	}
	id.clear ();
	return true;
}

bool appearance::isId () {
	return !isString && !id.empty () && getClass ( id [ 0 ] ) == Symbols::letter;
}

bool appearance::property () const {
	if ( startId == 0 ) {
		return false;
	}
	for ( auto i = startId; i > 0; ) {
		--i;
		auto symbol = getClass ( expression [ i ] );
		switch ( symbol ) {
		case Symbols::dot: {
			return true;
		}
		case Symbols::space:
		case Symbols::linebreak: {
			continue;
		}
		default: {
			return false;
		}
		}
	}
	return false;
}

void appearance::addField () {
	if ( !isId () ) {
		return;
	}
	if ( !property () ) {
		auto keyword = lower ();
		if ( !isKeyword ( keyword ) ) {
			if ( context == Contexts::expression ) {
				if ( isObject ( keyword ) ) {
					expression.replace ( startId, id.length (),
															 dataExtracter + L"(\"" + id + L"\")" );
				} else if ( !isReserved ( keyword ) ) {
					expression.replace ( startId, id.length (), fieldsPrefix + id );
					record.fields.emplace ( id );
				}
			} else {
				record.fields.emplace ( id );
			}
		}
	}
	id.clear ();
}

void appearance::addCollection () {
	if ( !isId () ) {
		return;
	}
	auto keyword = lower ();
	if ( !isKeyword ( keyword ) ) {
		if ( isObject ( keyword ) ) {
			expression.replace ( startId, id.length (),
													 dataExtracter + L"(\"" + id + L"\")" );
		} else if ( !isReserved ( keyword ) ) {
			expression.replace ( startId, id.length (), fieldsPrefix + id );
		}
	}
	id.clear ();
}

void appearance::adjustFunction () {
	if ( !isId () ) {
		return;
	}
	auto keyword = lower ();
	auto shifter = 0;
	if ( fillChecking ( keyword ) ) {
		expression.replace ( startId + shifter, id.length (), checkFillFunction );
	} else if ( voidChecking ( keyword ) ) {
		expression.replace ( startId, id.length () - shifter, checkVoidFunction );
	} else if ( builtin ( keyword ) ) {
		expression.replace ( startId - shifter, id.length () + shifter,
												 builtinPrefix + id );
	}
	id.clear ();
}

bool appearance::addDirective () {
	auto keyword = lower ();
	if ( isDirective ( keyword ) ) {
		if ( directive ( keyword ) ) {
			record.directive = keyword.back ();
			id.clear ();
			return true;
		}
		throw error { Errors::unknownDirective, position };
	}
	return false;
}

void appearance::harvestId () {
	if ( id.empty () ) {
		startId = expression.length () - 1;
	}
	id += item;
}

void appearance::harvestExpression () const {
	expression += item;
}

void appearance::startExpression () {
	context = Contexts::expression;
}

void appearance::init ( const std::wstring& rules ) {
	std::setlocale ( LC_CTYPE, "ru-RU" );
	this->rules = rules;
	if ( !strings::ends ( this->rules, L";" ) ) {
		this->rules += L";";
	}
	records.clear ();
	record.clear ();
	id.clear ();
	context = Contexts::controls;
	lastCharacter = Symbols::undefined;
	position = 0;
	isString = false;
}

bool appearance::addAppearanceParams () {
	if ( id.empty () ) {
		return false;
	}
	auto& format = record.appearance;
	format.back () += L"/" + id;
	id.clear ();
	return true;
}

void appearance::extract () {
	auto increment = 1;
	for ( ; position < rules.size (); position += increment ) {
		item = rules [ position ];
		character = getClass ( item );
		switch ( character ) {
		case Symbols::letter: {
			switch ( context ) {
			case Contexts::expression: {
				if ( lastCharacter == Symbols::space ||
						 lastCharacter == Symbols::linebreak ) {
					addField ();
				}
				harvestExpression ();
				break;
			}
			}
			if ( !isString ) {
				harvestId ();
			}
			break;
		}
		case Symbols::quote: {
			switch ( context ) {
			case Contexts::expression: {
				isString = !isString;
				harvestExpression ();
				break;
			default:
				throw error { Errors::wrongCharacter, position };
			}
			}
			break;
		}
		case Symbols::number: {
			auto isID = isId ();
			switch ( context ) {
			case Contexts::controls:
			case Contexts::appearance:
			case Contexts::appearanceParams: {
				if ( isID ) {
					harvestId ();
				} else {
					throw error { Errors::wrongCharacter, position };
				}
				break;
			}
			case Contexts::expression: {
				if ( isID ) {
					harvestId ();
				}
				harvestExpression ();
				break;
			}
			}
			break;
		}
		case Symbols::slash: {
			switch ( context ) {
			case Contexts::controls: {
				if ( addAppearance () ) {
					context = Contexts::appearanceParams;
				} else {
					throw error { Errors::wrongCharacter, position };
				}
				break;
			}
			case Contexts::appearance: {
				if ( addAppearance () ) {
					context = Contexts::appearanceParams;
				} else {
					throw error { Errors::unknownAppearance, position };
				}
				break;
			}
			case Contexts::expression:
				addField ();
				harvestExpression ();
				break;
			default:
				throw error { Errors::wrongCharacter, position };
			}
			break;
		}
		case Symbols::dot: {
			switch ( context ) {
			case Contexts::controls:
			case Contexts::appearanceParams: {
				if ( isId () ) {
					harvestId ();
				} else {
					throw error { Errors::wrongCharacter, position };
				}
				break;
			}
			case Contexts::expression:
				harvestExpression ();
				if ( isId () ) {
					harvestId ();
				}
				break;
			default:
				throw error { Errors::wrongCharacter, position };
			}
			break;
		}
		case Symbols::minus: {
			switch ( context ) {
			case Contexts::controls: {
				if ( addAppearance () ) {
					context = Contexts::appearance;
				} else {
					throw error { Errors::wrongCharacter, position };
				}
				break;
			}
			case Contexts::appearance: {
				if ( !addAppearance () ) {
					throw error { Errors::unknownAppearance, position };
				}
				break;
			}
			case Contexts::appearanceParams: {
				if ( addAppearanceParams () ) {
					context = Contexts::appearance;
				} else {
					throw error { Errors::unknownAppearanceParam, position };
				}
				break;
			}
			case Contexts::expression:
				addField ();
				harvestExpression ();
				break;
			default:
				throw error { Errors::wrongCharacter, position };
			}
			break;
		}
		case Symbols::hash: {
			switch ( context ) {
			case Contexts::controls: {
				if ( id.empty () ) {
					harvestId ();
				} else {
					throw error { Errors::wrongCharacter, position };
				}
				break;
			}
			case Contexts::expression:
				harvestExpression ();
				break;
			default:
				throw error { Errors::wrongCharacter, position };
			}
			break;
		}
		case Symbols::space:
		case Symbols::linebreak: {
			switch ( context ) {
			case Contexts::controls: {
				if ( id.empty () || addDirective () ) {
					break;
				}
				if ( addAppearance () ) {
					startExpression ();
				} else {
					addControl ();
				}
				break;
			}
			case Contexts::appearance: {
				if ( addAppearance () ) {
					startExpression ();
				} else {
					throw error { Errors::unknownAppearance, position };
				}
				break;
			}
			case Contexts::appearanceParams: {
				if ( addAppearanceParams () ) {
					startExpression ();
				} else {
					throw error { Errors::unknownAppearanceParam, position };
				}
				break;
			}
			case Contexts::expression: {
				harvestExpression ();
			}
			}
			break;
		}
		case Symbols::semicolon: {
			switch ( context ) {
			case Contexts::expression: {
				if ( isString ) {
					harvestExpression ();
				} else {
					addField ();
					addRecord ();
					context = Contexts::controls;
				}
				break;
			}
			default: {
				if ( lastCharacter == Symbols::semicolon ||
						 lastCharacter == Symbols::linebreak ) {
				} else {
					throw error { Errors::wrongCharacter, position };
				}
			}
			}
			break;
		}
		case Symbols::openingParenthesis: {
			switch ( context ) {
			case Contexts::expression: {
				adjustFunction ();
				harvestExpression ();
				break;
			}
			default:
				throw error { Errors::wrongCharacter, position };
			}
			break;
		}
		case Symbols::openingBracket: {
			switch ( context ) {
			case Contexts::expression: {
				addCollection ();
				harvestExpression ();
				break;
			}
			default:
				throw error { Errors::wrongCharacter, position };
			}
			break;
		}
		default: {
			switch ( context ) {
			case Contexts::expression: {
				addField ();
				harvestExpression ();
				break;
			}
			default:
				throw error { Errors::wrongCharacter, position };
			}
		}
		}
		lastCharacter = character;
	}
}

std::wstring appearance::convert () {
	std::wstring result { L"[" };
	auto splitter = false;
	for ( const auto& item : records ) {
		result += splitter ? L",{" : L"{";
		result += L"\"Directive\":\"" + item.directive + L"\",";
		result += L"\"Controls\":" +
							strings::listToJson<std::vector<std::wstring>> ( item.controls );
		result +=
				L",\"Appearance\":" +
				strings::listToJson<std::vector<std::wstring>> ( item.appearance );
#if __linux__
		result +=
				L",\"Fields\":" +
				strings::listToJson<std::unordered_set<std::wstring>> ( item.fields );
#else
		result += L",\"Fields\":" +
							strings::listToJson<std::pmr::unordered_set<std::wstring>> (
									item.fields );
#endif
		result += L",\"Expression\":\"" +
							strings::replace ( item.expression, L"\"", L"\\\"" ) + L"\"";
		result += L"}";
		splitter = true;
	}
	return result + L"]";
}

std::wstring appearance::parse ( const decltype ( rules )& rules ) {
	init ( rules );
	try {
		extract ();
	} catch ( error e ) {
		return L"{'Error':\"" + e.message + L"\",'Position':" +
					 std::to_wstring ( e.position + 1 ) + L"}";
	}
	return convert ();
}
}
