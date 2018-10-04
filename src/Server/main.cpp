#include "Server.h"

GUI_APP_MAIN {
	SetIniFile(ConfigFile("Server.ini"));
	
	Server s;
	s.Init();
	s.StartListen();
	s.Run();
}
