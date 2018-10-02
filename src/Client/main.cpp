#include "Client.h"

#define IMAGECLASS Images
#define IMAGEFILE <Client/Client.iml>
#include <Draw/iml_source.h>


GUI_APP_MAIN {
	SetIniFile(ConfigFile("Client.ini"));
	
	Client c;
	ServerDialog server(c);
	SettingsDialog settings(c);
	
	if (!server.IsAutoConnect()) {
		server.Run();
	} else {
		if (!server.Connect(true))
			server.Run();
	}
	server.Close();
	
	if (c.IsConnected()) {
		if (settings.IsFirstStart()) {
			if (settings.Run() != IDOK)
				return;
		}
		settings.Setup();
		settings.Close();
		
		c.Start();
		c.Run();
	}
}
