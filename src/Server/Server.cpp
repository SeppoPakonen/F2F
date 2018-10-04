#include "Server.h"

namespace Config {

INI_STRING(server_title, "Unnamed server", "Server name");
INI_INT(port, 17000, "Server port");
INI_INT(max_sessions, 10000, "Max clients");
INI_INT(max_image_size, 100000, "Max profile image size in bytes");
INI_INT(max_set_string_len, 1000000, "Max set function string size in bytes");
INI_STRING(master_addr, "93.170.105.68", "Master server's address");
INI_INT(master_port, 17123, "Master server's port");

};

String RandomPassword(int length) {
	String s;
	for(int i = 0; i < length; i++) {
		switch (Random(3)) {
			case 0: s.Cat('0' + Random(10)); break;
			case 1: s.Cat('a' + Random(25)); break;
			case 2: s.Cat('A' + Random(25)); break;
		}
	}
	return s;
}




Server::Server() {
	Title("F2F Server");
	
	usermode.Add("Active sessions");
	usermode.Add("All users");
	usermode.SetIndex(0);
	userlist.AddColumn("User-id");
	userlist.AddColumn("Name");
	userlist <<= THISBACK(Data);
	userctrl.Add(usermode.TopPos(0, 30).HSizePos());
	userctrl.Add(userlist.VSizePos(30).HSizePos());
	usersesslist.AddColumn("Begin");
	usersesslist.AddColumn("End");
	usersesslist <<= THISBACK(Data);
	userlog.AddColumn("Time");
	userlog.AddColumn("Message");
	userlog.ColumnWidths("1 4");
	usertabs.Add(userlog.SizePos(), "Log");
	split << userctrl << usersesslist << usertabs;
	split.SetPos(2500, 0);
	split.SetPos(5000, 1);
	split.Horz();
	tabs.Add(split.SizePos(), "Users");
	tabs.Add(serverlog.SizePos(), "Server log");
	
	serverlog.AddColumn("Time");
	serverlog.AddColumn("Message");
	serverlog.ColumnWidths("1 6");
	
	Add(tabs.SizePos());
	tabs.WhenSet << THISBACK(Data);
}

Server::~Server() {
	running = false; listener.Close(); while (!stopped) Sleep(100);
}

void Server::Print(const String& s) {
	LOG(s);
	log.Add().Set(s);
	if (tabs.Get() == SRVLOG_TAB)
		PostCallback(THISBACK(Data));
}

void Server::Init() {
	db.Init();
}

void Server::Data() {
	int tab = tabs.Get();
	
	if (tab == USER_TAB) {
		int mode = usermode.GetIndex();
		if (mode == 0) {
			int row = 0;
			for(int i = 0; i < sessions.GetCount(); i++) {
				const ActiveSession& s = sessions[i];
				UserDatabase& db = GetDatabase(s.user_id);
				userlist.Set(row, 0, s.user_id);
				userlist.Set(row, 1, db.name);
				row++;
			}
			userlist.SetCount(row);
		}
		else if (mode == 1) {
			
		}
		
		int user_cursor = userlist.GetCursor();
		if (user_cursor >= 0 && user_cursor < userlist.GetCount()) {
			int user_id = userlist.Get(user_cursor, 0);
			
			const UserDatabase& db = GetDatabase(user_id);
			
			for(int i = 0; i < db.sessions.GetCount(); i++) {
				const UserSessionLog& usl = db.sessions[i];
				usersesslist.Set(i, 0, usl.begin);
				usersesslist.Set(i, 1, usl.begin < usl.end ? Format("%", usl.end) : "");
			}
			usersesslist.SetCount(db.sessions.GetCount());
			
			int usersess_cursor = usersesslist.GetCursor();
			if (usersess_cursor >= 0 && usersess_cursor < db.sessions.GetCount()) {
				const UserSessionLog& usl = db.sessions[usersess_cursor];
				
				if (usertabs.Get() == 0) {
					for(int i = 0; i < usl.log.GetCount(); i++) {
						const LogItem& li = usl.log[i];
						userlog.Set(i, 0, li.added);
						userlog.Set(i, 1, li.msg);
					}
					userlog.SetCount(usl.log.GetCount());
				}
			}
		}
	}
	else if (tab == SRVLOG_TAB) {
		for(int i = serverlog.GetCount(); i < log.GetCount(); i++) {
			const LogItem& l = log[i];
			serverlog.Set(i, 0, l.added);
			serverlog.Set(i, 1, l.msg);
		}
		serverlog.ScrollEnd();
	}
	
}

void Server::Listen() {
	try {
		if(!listener.Listen(Config::port, 5)) {
			throw Exc("Unable to initialize server socket!");
			SetExitCode(1);
			return;
		}
		
		Print("Registering server to the master server list");
		TcpSocket master;
		master.Timeout(10000);
		if (!master.Connect((String)Config::master_addr, Config::master_port))
			throw Exc("Unable to connect the master server list");
		#ifdef flagPOSIX
		Sleep(1000); // weird bug occurs in linux without this
		#endif
		uint16 port = Config::port;
		int r = master.Put(&port, sizeof(uint16));
		if (r != sizeof(uint16)) throw Exc("Master server connection failed (1)");
		
		bool success = false;
		for(int i = 0; i < 1000; i++) {
			TcpSocket verify;
			verify.Timeout(10000);
			if (!verify.Accept(listener)) continue;
			int chk;
			r = verify.Get(&chk, sizeof(int));
			if (r != sizeof(int) || chk != 12345678) continue;
			r = verify.Put(&chk, sizeof(int));
			if (r != sizeof(int)) continue;
			
			int ret;
			r = master.Get(&ret, sizeof(int));
			if (r != sizeof(int)) continue;
			if (ret != 0) continue;
			master.Close();
			
			success = true;
			break;
		}
		if (!success) throw Exc("Master server couldn't connect this server");
		
		Print("Waiting for requests..");
		TimeStop ts;
		while (!Thread::IsShutdownThreads() && running) {
			One<ActiveSession> ses;
			ses.Create();
			ses->server = this;
			if(ses->s.Accept(listener)) {
				ses->s.Timeout(30000);
				
				// Close blacklisted connections
				if (blacklist.Find(ses->s.GetPeerAddr()) != -1)
					ses->s.Close();
				else {
					int id = session_counter++;
					ses->sess_id = id;
					
					int pre_cmd;
					ses->s.Get(&pre_cmd, sizeof(int));
					
					// Is greeting request
					if (pre_cmd == 0) {
						String title = Config::server_title;
						int i = title.GetCount();
						ses->s.Put(&i, sizeof(int));
						ses->s.Put(title.Begin(), title.GetCount());
						i = sessions.GetCount();
						ses->s.Put(&i, sizeof(int));
						i = Config::max_sessions;
						ses->s.Put(&i, sizeof(int));
						ses->s.Close();
					}
					// Is normal session
					else if (sessions.GetCount() < Config::max_sessions) {
						lock.EnterWrite();
						sessions.Add(id, ses.Detach()).Start();
						lock.LeaveWrite();
						
						if (tabs.Get() == USER_TAB)
							PostCallback(THISBACK(Data));
					}
				}
			}
			
			if (ts.Elapsed() > 60*1000) {
				lock.EnterWrite();
				for(int i = 0; i < sessions.GetCount(); i++) {
					if (!sessions[i].s.IsOpen()) {
						sessions.Remove(i);
						i--;
					}
				}
				lock.LeaveWrite();
				ts.Reset();
				
				if (tabs.Get() == USER_TAB)
					PostCallback(THISBACK(Data));
			}
		}
	}
	catch (Exc e) {
		Print(e);
		SetExitCode(1);
	}
	
	for(int i = 0; i < sessions.GetCount(); i++)
		sessions[i].s.Close();
	
	for(int i = 0; i < sessions.GetCount(); i++)
		while (!sessions[i].stopped)
			Sleep(100);
	
	stopped = true;
}

void Server::SendToAll(ActiveSession& user, String msg) {
	lock.EnterWrite();
	Index<int> users;
	user.GetUserlist(users);
	SendMessage(user.user_id, msg, users);
	lock.LeaveWrite();
}

void Server::JoinChannel(const String& channel, ActiveSession& user) {
	if (channel.IsEmpty()) return;
	lock.EnterWrite();
	int i = channel_ids.Find(channel);
	int id;
	if (i == -1) {
		id = channel_counter++;
		channel_ids.Add(channel, id);
		Channel& ch = channels.Add(id);
		ch.name = channel;
	} else {
		id = channel_ids[i];
	}
	Channel& ch = channels.Get(id);
	SendMessage(user.user_id, "join " + IntStr(user.user_id) + " " + channel, ch.users);
	ch.users.FindAdd(user.user_id);
	user.channels.Add(id);
	lock.LeaveWrite();
}

void Server::LeaveChannel(const String& channel, ActiveSession& user) {
	lock.EnterWrite();
	int id = channel_ids.GetAdd(channel);
	Channel& ch = channels.Get(id);
	ch.users.RemoveKey(user.user_id);
	user.channels.RemoveKey(id);
	SendMessage(user.user_id, "leave " + IntStr(user.user_id) + " " + channel, ch.users);
	UserDatabase& db = GetDatabase(user.user_id);
	lock.LeaveWrite();
}

void Server::SendMessage(int sender_id, const String& msg, const Index<int>& user_list) {
	ASSERT(sender_id >= 0);
	if (msg.IsEmpty()) return;
	const MessageRef& ref = IncReference(msg, user_list.GetCount());
	for(int i = 0; i < user_list.GetCount(); i++) {
		int user_id = user_list[i];
		if (user_id == sender_id) continue;
		int j = user_session_ids.Find(user_id);
		if (j == -1) continue;
		ActiveSession& as = sessions.Get(user_session_ids[j]);
		as.lock.Enter();
		InboxMessage& m = as.inbox.Add();
		m.sender_id = sender_id;
		m.msg = ref.hash;
		as.lock.Leave();
	}
}

const MessageRef& Server::IncReference(const String& msg, int ref_count) {
	unsigned hash = msg.GetHashValue();
	msglock.EnterWrite();
	MessageRef& ref = messages.GetAdd(hash);
	if (ref.msg.IsEmpty()) {
		ref.msg = msg;
		ref.hash = hash;
	}
	ref.refcount += ref_count;
	msglock.LeaveWrite();
	return ref;
}

#ifdef flagDEBUG
VectorMap<unsigned, String> message_history;
#endif

MessageRef& Server::GetReference(unsigned hash) {
	msglock.EnterRead();
	int i = messages.Find(hash);
	if (i == -1) {
		msglock.LeaveRead();
		#ifdef flagDEBUG
		i = message_history.Find(hash);
		if (i != -1)
			throw Exc("Message removed: " + message_history[i]);
		else
			throw Exc("Message removed. NEVER EXISTED");
		#endif
		throw Exc("Message removed");
	}
	MessageRef& ref = messages[i];
	msglock.LeaveRead();
	return ref;
}

void Server::DecReference(MessageRef& ref) {
	ref.refcount--;
	if (ref.refcount == 0) {
		msglock.EnterWrite();
		// check again after acquiring the lock
		if (ref.refcount == 0) {
			#ifdef flagDEBUG
			message_history.GetAdd(ref.hash, ref.msg);
			#endif
			messages.RemoveKey(ref.hash);
		}
		msglock.LeaveWrite();
	}
}



