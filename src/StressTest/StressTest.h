#ifndef _StressTest_StressTest_h
#define _StressTest_StressTest_h

#include <Core/Core.h>

using namespace Upp;

struct Location {
	double latitude, longitude, elevation;
};

struct User : Moveable<User> {
	// Metadata
	int user_id = -1;
	String name;
	bool is_updated = false;
	Time last_update;
	
	// Detailed information
	Vector<String> channels;
};

class Client {
	VectorMap<String, User> users;
	Index<String> joined_channels;
	String pass, sesspass;
	One<TcpSocket> s;
	int id = -1;
	int user_id = -1;
	
	
	String RandomNewChannel();
	String RandomOldChannel();
	int RandomUser();
	String RandomMessage();
	Location RandomLocation();
	
public:
	typedef Client CLASSNAME;
	Client();
	
	void SetId(int i) {id = i;}
	
	void Run();
	void Start() {Thread::Start(THISBACK(Run));}
	void Call(Stream& out, Stream& in);
	
	void Register();
	void Login();
	void Join(String channel);
	void Leave(String channel);
	void SendLocation(const Location& elevation);
	void Message(int recv_user_id, const String& msg);
	bool Set(const String& key, const String& value);
	void Get(const String& key, String& value);
	void Poll();
	void RefreshUserlist();
};

#endif
