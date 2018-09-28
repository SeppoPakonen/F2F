#ifndef _Client_Client_h
#define _Client_Client_h

#include <CtrlLib/CtrlLib.h>
#include "GoogleMaps.h"
using namespace Upp;

#define LAYOUTFILE <Client/Client.lay>
#include <CtrlCore/lay.h>

#define IMAGECLASS Images
#define IMAGEFILE <Client/Client.iml>
#include <Draw/iml_header.h>


void Print(const String& s);

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
};

struct ChannelMessage : Moveable<ChannelMessage> {
	int sender_id;
	String message, sender_name;
	Time received;
};

struct Channel : Moveable<Channel> {
	Vector<ChannelMessage> messages;
	Index<int> userlist;
	int unread = 0;
	
	void Post(int user_id, String user_name, const String& msg);
};

class ChannelList : public ParentCtrl {
	Array<Button> buttons;
	Button::Style unread_style;
	
	void SetChannel(int i) {WhenChannel(buttons[i].GetLabel());}
public:
	typedef ChannelList CLASSNAME;
	ChannelList();
	virtual void Layout();
	void SetChannelCount(int i);
	void SetChannelName(int i, String s);
	void SetChannelUnread(int i, int unread);
	
	Callback1<String> WhenChannel;
};

class IrcCtrl : public ParentCtrl {
	ChannelList channels;
	ArrayCtrl chat, users;
	EditString cmd;
	
	String active_channel;
	
	void Command();
	void SetChannel(String s) {active_channel = s; chat.Clear(); users.Clear(); WhenChannelChanged();}
	
public:
	typedef IrcCtrl CLASSNAME;
	IrcCtrl();
	
	ChannelList& GetChannelList() {return channels;}
	String GetActiveChannel() const {return active_channel;}
	void Post(const ChannelMessage& msg);
	void SetUserCount(int i);
	void SetUser(int i, String s);
	void SetActiveChannel(String s) {active_channel = s;}
	
	Callback1<String> WhenCommand;
	Callback WhenChannelChanged;
};

class Client : public TopWindow {
	
	// Session
	ArrayMap<String, Channel> channels;
	ArrayMap<int, User> users;
	Index<String> my_channels;
	String user_name, pass;
	String addr = "127.0.0.1";
	One<TcpSocket> s;
	int user_id = -1;
	int port = 17000;
	bool is_registered = false, is_logged_in = false;
	bool running = false, stopped = true;
	Mutex lock;
	
	
	Splitter split;
	IrcCtrl irc;
	MapDlgDlg map;
	
public:
	typedef Client CLASSNAME;
	Client();
	~Client();
	
	
	bool Connect();
	void CloseConnection() {if (!s.IsEmpty()) s->Close();}
	bool LoginScript();
	bool RegisterScript();
	void HandleConnection();
	void Start() {if (!stopped) return; stopped = false; running = true; Thread::Start(THISBACK(HandleConnection));}
	void Call(Stream& out, Stream& in);
	void SetAddress(String a, int p) {addr = a; port = p;}
	
	void Register();
	void Login();
	void Join(String channel);
	void Leave(String channel);
	void SendLocation(const Location& elevation);
	void Message(int recv_user_id, const String& msg);
	void SendChannelMessage(String channel, const String& msg);
	bool Set(const String& key, const String& value);
	void Get(const String& key, String& value);
	void Poll();
	void RefreshChannellist();
	void RefreshUserlist();
	void ChangeLocation(Pointf coord);
	
	void RefreshGui();
	void RefreshGuiChannel();
	void Command(String cmd);
	
	bool IsConnected() {return !s.IsEmpty() && s->IsOpen() && is_logged_in;}
	
};

class PreviewImage : public ImageCtrl {
	void SetData(const Value& val) {
		String path = val;
		if(IsNull(path.IsEmpty()))
			SetImage(Null);
		else
			SetImage(StreamRaster::LoadFileAny(~path));
	}
};

class StartupDialog : public WithStartupDialog<TopWindow> {
	
	// Persistent
	Image profile_image;
	String name, srv_addr;
	int srv_port;
	bool autoconnect = false;
	
	// Temporary
	Client& cl;
	bool connecting = false;
	
public:
	typedef StartupDialog CLASSNAME;
	StartupDialog(Client& c);
	
	void StartTryConnect() {Enable(false); Thread::Start(THISBACK(TryConnect));}
	void StartStopConnect() {Thread::Start(THISBACK(StopConnect));}
	void TryConnect();
	void StopConnect();
	void Close0() {Close();}
	void SetError(String e) {error.SetLabel(e);}
	void Enable(bool b);
	bool Connect();
	void SelectServer();
	void SelectImage();
	
	bool IsAutoConnect() const {return autoconnect;}
	
	void StoreThis() {StoreToFile(*this, ConfigFile("Startup.bin"));}
	void LoadThis() {LoadFromFile(*this, ConfigFile("Startup.bin"));}
	void Serialize(Stream& s) {s % profile_image % name % srv_addr % srv_port % autoconnect;}
	
};

class ServerDialog : public WithSelectServer<TopWindow> {
	StartupDialog& sd;
	
protected:
	friend class StartupDialog;
	
	struct Server : Moveable<Server> {
		String addr;
		uint16 port;
	};
	Vector<Server> servers;
	
	
public:
	typedef ServerDialog CLASSNAME;
	ServerDialog(StartupDialog& sd);
	
	void RefreshAddresses();
	void TestConnection(int i);
	
};

#endif
