#ifndef _Server_Server_h_
#define _Server_Server_h_

#include <Core/Core.h>
#include <plugin/sqlite3/Sqlite3.h>
using namespace Upp;

class Server;

void Print(const String& s);
String RandomPassword(int length);

struct InboxMessage : Moveable<InboxMessage> {
	int sender_id = 0;
	String message;
};

class ActiveSession {
	
protected:
	friend class Server;
	
	Server* server = NULL;
	SpinLock lock;
	Sql sql;
	TcpSocket s;
	
	int sess_id = -1;
	int user_id = -1;
	String name;
	
	Vector<InboxMessage> inbox;
	
	
public:
	typedef ActiveSession CLASSNAME;
	ActiveSession();
	void Run();
	void Start() {Thread::Start(THISBACK(Run));}
	
	
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

class Server {
	
protected:
	friend class ActiveSession;
	
	ArrayMap<int, ActiveSession> sessions;
	VectorMap<int, int> user_session_ids;
	TcpSocket listener;
	Index<String> blacklist;
	Sqlite3Session sqlite3;
	RWMutex lock;
	int session_counter = 0;
	
public:
	typedef Server CLASSNAME;
	Server();
	
	void Init();
	void Listen();
	void HandleSocket(One<TcpSocket> s);
	
};

#endif
