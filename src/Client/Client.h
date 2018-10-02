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
double CoordinateDistanceKM(Pointf a, Pointf b);

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
	unsigned profile_image_hash = 0;
	unsigned age = 0;
	bool gender = 1;
	Image profile_image;
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
	void SetUser(int i, Image img, String s);
	void SetActiveChannel(String s) {active_channel = s;}
	
	Callback1<String> WhenCommand;
	Callback WhenChannelChanged;
};

class Client : public TopWindow {
	
	// Persistent
	int user_id = -1;
	String pass;
	bool is_registered = false;
	
	// Session
	ArrayMap<String, Channel> channels;
	ArrayMap<int, User> users;
	ArrayMap<unsigned, Image> image_cache;
	Index<String> my_channels;
	String user_name;
	String addr = "127.0.0.1";
	One<TcpSocket> s;
	int port = 17000;
	int age = 0;
	bool gender = 0;
	bool is_logged_in = false;
	bool running = false, stopped = true;
	Mutex call_lock, lock;
	
	
	MenuBar menu;
	Splitter split, rvsplit, rhsplit;
	IrcCtrl irc;
	MapDlgDlg map;
	ArrayCtrl nearestlist;
	WithDetails<ParentCtrl> details;
	
public:
	typedef Client CLASSNAME;
	Client();
	~Client();
	
	int GetUserId() const {return user_id;}
	String GetPassword() const {return pass;}
	
	void Serialize(Stream& s) {s % user_id % pass % is_registered;}
	void StoreThis() {StoreToFile(*this, ConfigFile("Client" + IntStr64(GetServerHash()) + ".bin"));}
	void LoadThis() {LoadFromFile(*this, ConfigFile("Client" + IntStr64(GetServerHash()) + ".bin"));}
	unsigned GetServerHash() {CombineHash h; h << addr << port; return h;}
	
	void MainMenu(Bar& bar);
	bool Connect();
	void CloseConnection() {if (!s.IsEmpty()) s->Close(); is_logged_in = false;}
	bool LoginScript();
	bool RegisterScript();
	void HandleConnection();
	void Start() {if (!stopped) return; stopped = false; running = true; Thread::Start(THISBACK(HandleConnection));}
	void Call(Stream& out, Stream& in);
	void SetAddress(String a, int p) {addr = a; port = p;}
	void SetName(String s);
	void SetImage(Image i);
	void SetAge(int i);
	void SetGender(bool b);
	void StoreImageCache(const String& image_str);
	bool HasCachedImage(unsigned hash);
	String LoadImageCache(unsigned hash);
	
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
	void RefreshUserImage(User& u);
	bool Who(Stream& in);
	void ChangeLocation(Pointf coord);
	void JoinChannel();
	
	void RefreshGui();
	void RefreshGuiChannel();
	void RefreshNearest();
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

class ServerDialog;

class SelectServerDialog : public WithSelectServer<TopWindow> {
	ServerDialog& sd;
	
protected:
	friend class ServerDialog;
	
	struct Server : Moveable<Server> {
		String greeting, addr;
		uint16 port = 0;
		int elapsed = 9999, sessions = 0, max_sessions = 0;
	};
	Vector<Server> servers;
	Array<TcpSocket> socks;
	int running = 0;
	
public:
	typedef SelectServerDialog CLASSNAME;
	SelectServerDialog(ServerDialog& sd);
	~SelectServerDialog() {for(auto& s : socks) {s.Timeout(1); s.Close();} while (running > 0) Sleep(100);}
	
	void RefreshAddresses();
	void TestConnection(int i);
	void Data();
};

class RegisterDialog : public WithRegisterDialog<TopWindow> {
	ServerDialog& sd;
	
public:
	typedef RegisterDialog CLASSNAME;
	RegisterDialog(ServerDialog& sd);
	~RegisterDialog();
	
	void Register();
	void TryRegister();
};

class ServerDialog : public WithServerDialog<TopWindow> {
	
protected:
	friend class RegisterDialog;
	
	// Persistent
	String srv_addr;
	int srv_port;
	bool autoconnect = false;
	
	// Temporary
	Client& cl;
	bool connecting = false;
	SelectServerDialog sd;
	RegisterDialog rd;
	
public:
	typedef ServerDialog CLASSNAME;
	ServerDialog(Client& c);
	
	void StartTryConnect() {Enable(false); Thread::Start(THISBACK(TryConnect));}
	void StartStopConnect() {Thread::Start(THISBACK(StopConnect));}
	void TryConnect();
	void StopConnect();
	void Close0() {Close();}
	void SetError(String e) {error.SetLabel(e);}
	void Enable(bool b);
	bool Connect(bool do_login);
	void Register();
	void SelectServer();
	
	bool IsAutoConnect() const {return autoconnect;}
	
	void StoreThis() {StoreToFile(*this, ConfigFile("ClientServer.bin"));}
	void LoadThis() {LoadFromFile(*this, ConfigFile("ClientServer.bin"));}
	void Serialize(Stream& s) {s % srv_addr % srv_port % autoconnect;}
	
};

class SettingsDialog : public WithSettingsDialog<TopWindow> {
	
	// Persistent
	Image profile_image;
	String name;
	int age = 18;
	bool gender = 1;
	bool is_first_start = true;
	
	// Temporary
	Client& cl;
	
public:
	typedef SettingsDialog CLASSNAME;
	SettingsDialog(Client& cl);
	
	void Close0() {Close();}
	
	void SelectImage();
	bool Setup();
	
	bool IsFirstStart() const {return is_first_start;}
	
	void StoreThis() {StoreToFile(*this, ConfigFile("Settings.bin"));}
	void LoadThis() {LoadFromFile(*this, ConfigFile("Settings.bin"));}
	void Serialize(Stream& s) {s % profile_image % name % age % gender % is_first_start;}
	
};


#endif
