#include "Client.h"

#define IMAGEFILE <Client/Client.iml>
#include <Draw/iml_source.h>

Client::Client()
{
	CtrlLayout(*this, "Window title");
}

GUI_APP_MAIN
{
	Client().Run();
}
