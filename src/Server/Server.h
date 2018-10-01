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
	
	// Temporary
	Mutex lock;
	String user_file, user_loc_file;
	FileAppend location;
	int user_id = -1;
	bool open = false;
	
	
public:
	UserDatabase();
	int Init(int user_id);
	void Deinit();
	bool IsOpen() const {return open;}
	
	
	// Persistent
	Index<String> channels;
	String name, profile_img;
	unsigned profile_img_hash = 0;
	unsigned passhash = 0;
	Time joined, lastlogin;
	int64 logins = 0, onlinetotal = 0, visibletotal = 0;
	double longitude = 0, latitude = 0, elevation = 0;
	Time lastupdate;
	
	void Serialize(Stream& s) {s % name % profile_img % profile_img_hash % channels % passhash % joined % lastlogin % logins % onlinetotal % visibletotal % longitude % latitude % elevation % lastupdate;}
	void Flush();
	void SetLocation(double longitude, double latitude, double elevation);
};

enum {
	SERVER_NICK
};

class ServerDatabase {
	
	// Persistent
	VectorMap<int, String> users;
	
	// Temporary
	String srv_file;
	
public:
	ServerDatabase();
	
	void Init();
	int GetUserCount() {return users.GetCount();}
	void AddUser(int id, String user) {users.Add(id, user);}
	void SetUser(int user_id, String name) {users.GetAdd(user_id) = name;}
	String GetUser(int i) {return users[i];}
	
	void Serialize(Stream& s) {s % users;}
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

UserDatabase& GetDatabase(int user_id);

class ActiveSession {
	
protected:
	friend class Server;
	
	Server* server = NULL;
	Mutex lock;
	TcpSocket s;
	
	int sess_id = -1;
	int user_id = -1;
	
	Vector<InboxMessage> inbox;
	Index<int> channels;
	
	
public:
	typedef ActiveSession CLASSNAME;
	ActiveSession();
	void Run();
	void Start() {Thread::Start(THISBACK(Run));}
	void GetUserlist(Index<int>& userlist);
	void StoreImageCache(unsigned hash, const String& image_str);
	
	void Register(Stream& in, Stream& out);
	void Login(Stream& in, Stream& out);
	void Logout();
	void Join(Stream& in, Stream& out);
	void Leave(Stream& in, Stream& out);
	void Location(Stream& in, Stream& out);
	void Message(Stream& in, Stream& out);
	void ChannelMessage(Stream& in, Stream& out);
	void Poll(Stream& in, Stream& out);
	void Get(Stream& in, Stream& out);
	void Set(Stream& in, Stream& out);
	
	void Who(int user_id, Stream& out);
	
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
	void SendToAll(ActiveSession& user, String msg);
	const MessageRef&	IncReference(const String& msg, int ref_count);
	MessageRef&			GetReference(unsigned hash);
	void				DecReference(MessageRef& ref);
};

#endif
