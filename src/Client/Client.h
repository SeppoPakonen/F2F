#ifndef _Client_Client_h
#define _Client_Client_h

#include <CtrlLib/CtrlLib.h>

using namespace Upp;

#define LAYOUTFILE <Client/Client.lay>
#include <CtrlCore/lay.h>

#define IMAGEFILE <Client/Client.iml>
#include <Draw/iml_header.h>

class Client : public WithClientLayout<TopWindow> {
public:
	typedef Client CLASSNAME;
	Client();
};

#endif
