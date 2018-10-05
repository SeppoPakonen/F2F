#ifndef _StressTest_StressTest_h
#define _StressTest_StressTest_h

#include <Draw/Draw.h>
#include <plugin/jpg/jpg.h>
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
	Index<String> channels;
	double longitude = 0, latitude = 0, elevation = 0;
	int age = 0, gender = 0;
	unsigned profile_img_hash;
};

class Client {
	VectorMap<int, User> users;
	Index<String> joined_channels;
	String user_name, pass;
	One<TcpSocket> s;
	int age = 0, gender = 0;
	int id = -1;
	int user_id = -1;
	
	Location location;
	double radius, step, steps = 0;
	
	
	
	String RandomName();
	String RandomNewChannel();
	String RandomOldChannel();
	int RandomUser();
	String RandomMessage();
	Location RandomLocation();
	Location NextLocation();
	
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
	bool Message(int recv_user_id, const String& msg);
	bool Set(const String& key, const String& value);
	void Get(const String& key, String& value);
	void Poll();
	bool ChannelMessage(String channel, const String& msg);
	void RefreshChannellist();
	void RefreshUserlist();
	String RandomImage();
	
	Event<int, String> WhenMessage;
	
};

#endif
