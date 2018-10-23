#ifndef _Server_Server_h_
#define _Server_Server_h_

#include <CtrlLib/CtrlLib.h>
#include <MapCtrl/MapCtrl.h>
using namespace Upp;

#include "BotClient.h"


class Server;

namespace Config {

extern IniString server_title;
extern IniInt port;
extern IniInt max_sessions;
extern IniInt max_image_size;
extern IniInt max_set_string_len;
extern IniString master_addr;
extern IniInt master_port;

};

String RandomPassword(int length);
void DrawRect(ImageBuffer& ib, Rect r, RGBA rgba);

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

struct LogItem : Moveable<LogItem> {
	String msg;
	Time added;
	
	void Set(const String& s) {added = GetSysTime(); msg = s;}
	void Serialize(Stream& s) {s % msg % added;}
};

struct UserSessionLog : Moveable<LogItem> {
	Vector<LogItem> log;
	Time begin = Time(1970,1,1), end = Time(1970,1,1);
	String peer_addr;
	
	
	void Serialize(Stream& s) {s % log % begin % end % peer_addr;}
};

struct InboxMessage : Moveable<InboxMessage> {
	int sender_id;
	unsigned msg;
	
	void Serialize(Stream& s) {s % sender_id % msg;}
};

class UserDatabase {
	
protected:
	friend class ActiveSession;
	friend class Server;
	
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
	Vector<InboxMessage> inbox;
	ArrayMap<int, UserSessionLog> sessions;
	Index<int> channels;
	String name, profile_img;
	unsigned profile_img_hash = 0;
	unsigned passhash = 0;
	unsigned age = 0;
	Time joined, lastlogin;
	int64 logins = 0, onlinetotal = 0, visibletotal = 0;
	double longitude = 0, latitude = 0, elevation = 0;
	Time lastupdate;
	bool gender = 1;
	
	void Serialize(Stream& s) {s % inbox % sessions % channels % name % profile_img % profile_img_hash % channels % passhash % age % joined % lastlogin % logins % onlinetotal % visibletotal % longitude % latitude % elevation % lastupdate % gender;}
	void Flush();
	void SetLocation(double longitude, double latitude, double elevation);
};

struct Line : Moveable<Line> {
	Pointf a, b;
};

struct Channel : Moveable<Channel> {
	Index<int> users;
	String name;
	int id;
	
	void Serialize(Stream& s) {s % users % name % id;}
};

class ServerDatabase {
	
protected:
	friend class Server;
	friend class ActiveSession;
	
	// Persistent
	VectorMap<int, String> users;
	VectorMap<String, Channel> channels;
	
	// Temporary
	String srv_file;
	
public:
	ServerDatabase();
	
	void Init();
	int GetUserCount() {return users.GetCount();}
	void AddUser(int id, String user) {users.Add(id, user);}
	void SetUser(int user_id, String name) {users.GetAdd(user_id) = name;}
	String GetUser(int i) {return users[i];}
	
	void Serialize(Stream& s) {s % users % channels;}
	void Flush();
	
};

UserDatabase& GetDatabase(int user_id);

class ActiveSession {
	
protected:
	friend class Server;
	
	Server* server = NULL;
	TcpSocket s;
	bool stopped = false;
	
	int sess_id = -1;
	int last_user_id = -1;
	
	Vector<LogItem> log;
	
public:
	typedef ActiveSession CLASSNAME;
	ActiveSession();
	void Print(const String& s);
	void Run();
	void Start() {Thread::Start(THISBACK(Run));}
	void Stop() {s.Close();}
	
	void StoreImageCache(unsigned hash, const String& image_str);
	void DereferenceMessages();
	
	void Greeting(Stream& in, Stream& out);
	void Register(Stream& in, Stream& out);
	void Login(Stream& in, Stream& out);
	void Logout();
	int  LoginId(Stream& in);
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

class Server : public TopWindow {
	
protected:
	friend class ActiveSession;
	
	// Persistent
	int session_counter = 0;
	Vector<LogItem> log;
	
	
	// Temporary
	ArrayMap<int, ActiveSession> sessions;
	VectorMap<int, int> user_session_ids;
	VectorMap<int64, int> login_session_ids;
	VectorMap<unsigned, MessageRef> messages;
	TcpSocket listener;
	Index<String> blacklist;
	ServerDatabase db;
	RWMutex lock, msglock;
	bool running = false, stopped = true;
	
	
	
	Array<Client> clients;
	TimeCallback tc;
	
	MenuBar menubar;
	
	DropList usermode;
	ArrayCtrl userlist;
	ParentCtrl userctrl;
	ArrayCtrl usersesslist;
	ArrayCtrl userchannels, userdetails;
	Splitter usermainsplit;
	ArrayCtrl userlog;
	TabCtrl usertabs;
	Splitter split;
	
	ArrayCtrl serverlog;
	
	ArrayCtrl analyze_chlist;
	MapDlgDlg analyze_mapctrl;
	ParentCtrl analyzectrl;
	
	enum {USER_TAB, SRVLOG_TAB, ANAL_TAB};
	TabCtrl tabs;
	
public:
	typedef Server CLASSNAME;
	Server();
	~Server();
	
	void StoreThis() {StoreToFile(*this, ConfigFile("Server.bin"));}
	void LoadThis() {LoadFromFile(*this, ConfigFile("Server.bin"));}
	void Serialize(Stream& s) {s % session_counter % log;}
	
	void TimedRefresh();
	void MainMenu(Bar& bar);
	void AddBots();
	void RemoveBots();
	void CloseSession();
	void Analyze(String ch);
	void ChangeLocation();
	
	void Print(const String& s);
	void Data();
	
	void Init();
	void Listen();
	void StartListen() {running = true; stopped = false; Thread::Start(THISBACK(Listen));}
	void HandleSocket(One<TcpSocket> s);
	
	void JoinChannel(const String& channel, int user_id);
	void LeaveChannel(const String& channel, int user_id);
	void SendMessage(int sender_id, const String& msg, const Index<int>& user_list);
	void SendToAll(int user_id, String msg);
	void GetUserlist(Index<int>& userlist, int user_id);
	int64 GetNewLoginId();
	const MessageRef&	IncReference(const String& msg, int ref_count);
	MessageRef&			GetReference(unsigned hash);
	void				DecReference(MessageRef& ref);
};

#endif
