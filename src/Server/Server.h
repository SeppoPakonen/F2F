#ifndef _Server_Server_h_
#define _Server_Server_h_

#include <Core/Core.h>
#include <plugin/sqlite3/Sqlite3.h>
using namespace Upp;

String RandomPassword(int length);

struct InboxMessage : Moveable<InboxMessage> {
	int sender_id = 0;
	String message;
};

struct ActiveSession {
	int user_id = 0;
	String name;
	Vector<InboxMessage> inbox;
	SpinLock lock;
};

class Server {
	One<Sql> sql;
	ArrayMap<String, ActiveSession> sessions;
	VectorMap<int, String> user_session_ids;
	TcpSocket listener;
	Index<String> blacklist;
	Sqlite3Session sqlite3;
	Mutex lock;
	
public:
	typedef Server CLASSNAME;
	Server();
	
	void Init();
	void Listen();
	void HandleSocket(One<TcpSocket> s);
	
	void Register(Stream& in, Stream& out);
	void Login(Stream& in, Stream& out);
	void Join(Stream& in, Stream& out);
	void Leave(Stream& in, Stream& out);
	void Location(Stream& in, Stream& out);
	void Message(Stream& in, Stream& out);
	void Poll(Stream& in, Stream& out);
	void Get(Stream& in, Stream& out);
	void Set(Stream& in, Stream& out);
	
};

#endif
