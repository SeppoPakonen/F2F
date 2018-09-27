#ifndef _Master_Master_h
#define _Master_Master_h

#include <Core/Core.h>
using namespace Upp;

void Print(const String& s);

struct Server : Moveable<Server> {
	String addr;
	uint16 port;
	
	void Serialize(Stream& s) {s % addr % port;}
};

class Master {
	
	// Persistent
	Vector<Server> servers;
	
	// Temporary
	TcpSocket listener;
	RWMutex lock;
	
public:
	typedef Master CLASSNAME;
	Master();
	
	void Run();
	void Start() {Thread::Start(THISBACK(Run));}
	void Session(One<TcpSocket> t);
	
	void Serialize(Stream& s) {s % servers;}
	void StoreThis() {StoreToFile(*this, ConfigFile("master.bin"));}
	void LoadThis() {LoadFromFile(*this, ConfigFile("master.bin"));}
};

#endif
