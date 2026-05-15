#include "1c/componentbase.h"
#include "chars.h"
#include "metadata.h"
#include "regex.h"
#include "root.h"
#include "watcher.h"
#include "MCPServer.h"
#if _WIN32
[[maybe_unused]]
BOOL APIENTRY DllMain ( [[maybe_unused]] HMODULE hModule,
												[[maybe_unused]] DWORD ul_reason_for_call,
												[[maybe_unused]] LPVOID lpReserved ) {
	return TRUE;
}
#endif

const WCHAR_T *GetClassNames () {
	static const std::u16string Classes (
			u"Root|Watcher|Regex|MCPServer|Metadata" );
	return Classes.c_str ();
}

long GetClassObject ( const WCHAR_T* Name, IComponentBase** Interface ) {
	if ( *Interface ) {
		return 0;
	}
	const std::basic_string_view<WCHAR_T> module ( Name );
	if ( module == u"Root" ) {
		*Interface = new Root ();
	} else if ( module == u"Watcher" ) {
		try {
			*Interface = new Watcher ();
		} catch ( ... ) {
			return 0;
		}
	} else if ( module == u"Regex" ) {
		*Interface = new Regex ();
	} else if ( module == u"MCPServer" ) {
		*Interface = new MCPServer ();
	} else if ( module == u"Metadata" ) {
		*Interface = new Metadata ();
	}
	return reinterpret_cast<long> ( *Interface );
}

AppCapabilities SetPlatformCapabilities ( const AppCapabilities ) {
	return eAppCapabilitiesLast;
}

long DestroyObject ( IComponentBase** Interface ) {
	if ( !*Interface ) {
		return -1;
	}
	delete *Interface;
	*Interface = nullptr;
	return 0;
}
