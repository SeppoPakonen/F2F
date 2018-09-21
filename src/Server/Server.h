#ifndef _Server_Server_h_
#define _Server_Server_h_

#include <Core/Core.h>
using namespace Upp;

class Server;

void Print(const String& s);
String RandomPassword(int length);

enum {
	NICK,
	PASSHASH,
	JOINED,
	LASTLOGIN,
	LOGINS,
	ONLINETOTAL,
	VISIBLETOTAL,
	LL_LON, LL_LAT, LL_ELEVATION,
	LL_UPDATED,
	CHANNEL_BEGIN
	
};


class UserDatabase {
	int user_id = -1;
	FileAppend user, location;
	Vector<int> offsets;
	int record_size = 0;
	bool open = false;
	
	static const int channel_record = 200+sizeof(Time);
	
public:
	UserDatabase();
	int Init(int user_id);
	void Deinit();
	void Value(int size) {offsets.Add(record_size); record_size += size;}
	bool IsOpen() const {return open;}
	
	void SetTime(int key, Time value);
	void SetStr(int key, String value);
	void SetInt(int key, int64 value);
	void SetDbl(int key, double value);
	
	void Flush();
	
	int64 GetInt(int key);
	String GetString(int key);
	Time GetTime(int key);
	double GetDbl(int key);
	
	int AddChannel(const String& channel);
	int GetChannelCount();
	String GetChannel(int i);
	int FindChannel(const String& channel);
	void DeleteChannel(int i);
	
	void SetLocation(double longitude, double latitude, double elevation);
};

enum {
	SERVER_NICK
};

class ServerDatabase {
	FileAppend file;
	int user_count = 0;
	
	static const int user_record = 200;
	
public:
	ServerDatabase();
	
	void Init();
	int GetUserCount();
	void AddUser(String user);
	int FindUser(const String& user);
	void SetUser(int user_id, String name);
	String GetUser(int i);
	
	void Flush();
	
};

struct InboxMessage : Moveable<InboxMessage> {
	int sender_id;
	unsigned msg;
};

struct Channel {
	Index<int> users;
	String name;
	int id;
};

class ActiveSession {
	
protected:
	friend class Server;
	
	Server* server = NULL;
	SpinLock lock;
	TcpSocket s;
	
	int sess_id = -1;
	int user_id = -1;
	String name;
	
	Vector<InboxMessage> inbox;
	Index<int> channels;
	
	UserDatabase db;
	
public:
	typedef ActiveSession CLASSNAME;
	ActiveSession();
	void Run();
	void Start() {Thread::Start(THISBACK(Run));}
	void GetUserlist(Index<int>& userlist);
	
	void Register(Stream& in, Stream& out);
	void Login(Stream& in, Stream& out);
	void Logout();
	void Join(Stream& in, Stream& out);
	void Leave(Stream& in, Stream& out);
	void Location(Stream& in, Stream& out);
	void Message(Stream& in, Stream& out);
	void Poll(Stream& in, Stream& out);
	void Get(Stream& in, Stream& out);
	void Set(Stream& in, Stream& out);
};

struct MessageRef : Moveable<MessageRef> {
	String msg;
	Atomic refcount;
	unsigned hash;
	
	MessageRef() {refcount = 0;}
};

class Server {
	
protected:
	friend class ActiveSession;
	
	ArrayMap<int, ActiveSession> sessions;
	ArrayMap<int, Channel> channels;
	VectorMap<int, int> user_session_ids;
	VectorMap<String, int> channel_ids;
	VectorMap<unsigned, MessageRef> messages;
	TcpSocket listener;
	Index<String> blacklist;
	ServerDatabase db;
	RWMutex lock, msglock;
	int channel_counter = 0;
	int session_counter = 0;
	
public:
	typedef Server CLASSNAME;
	Server();
	
	void Init();
	void Listen();
	void HandleSocket(One<TcpSocket> s);
	
	void JoinChannel(const String& channel, ActiveSession& user);
	void LeaveChannel(const String& channel, ActiveSession& user);
	void SendMessage(int sender_id, const String& msg, const Index<int>& user_list);
	const MessageRef&	IncReference(const String& msg, int ref_count);
	MessageRef&			GetReference(unsigned hash);
	void				DecReference(MessageRef& ref);
};

#endif
