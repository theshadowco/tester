#ifndef __metadata_h__
#define __metadata_h__
#include "extender.h"
#include "tooltips.h"

class Metadata : public Extender {
public:
	Metadata ();

private:
	bool getTooltips ( tVariant* Params, tVariant* Result );
};
#endif
