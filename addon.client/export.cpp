#include "chars.h"
#include "1c/componentbase.h"
#include "httpServer.h"

#if _WIN32
[[maybe_unused]]
BOOL APIENTRY DllMain ( [[maybe_unused]] HMODULE hModule,
												[[maybe_unused]] DWORD ul_reason_for_call,
												[[maybe_unused]] LPVOID lpReserved ) {
	return TRUE;
}
#endif

static const wchar_t ClassNames [] = L"httpServer";
static WCHAR_T* Names {};

long GetClassObject ( const WCHAR_T* Name, IComponentBase** Interface ) {
	if ( *Interface ) {
		return 0;
	}
	const std::basic_string_view<WCHAR_T> module ( Name );
	if ( module == u"httpServer" ) {
		*Interface = new HTTPServer ();
	}
	return reinterpret_cast<long> ( *Interface );
}

AppCapabilities SetPlatformCapabilities ( const AppCapabilities ) {
	return eAppCapabilitiesLast;
}

AttachType GetAttachType () {
	return eCanAttachAny;
}

long DestroyObject ( IComponentBase** Interface ) {
	if ( !*Interface ) {
		return -1;
	}
	delete *Interface;
	*Interface = nullptr;
	return 0;
}

const WCHAR_T* GetClassNames () {
	if ( !Names ) {
		Chars::ToWCHAR ( &Names, ClassNames );
	}
	return Names;
}
