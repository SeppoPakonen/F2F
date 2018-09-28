#include "Client.h"

#define IMAGECLASS Images
#define IMAGEFILE <Client/Client.iml>
#include <Draw/iml_source.h>


GUI_APP_MAIN {
	SetIniFile(ConfigFile("Client.ini"));
	
	Client c;
	StartupDialog startup(c);
	
	if (!startup.IsAutoConnect()) {
		startup.Run();
	} else {
		if (!startup.Connect())
			startup.Run();
	}
	
	if (c.IsConnected()) {
		startup.Setup();
		c.Start();
		c.Run();
	}
}
