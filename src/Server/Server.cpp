#include "Server.h"
#include "AES.h"


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

void Print(const String& s) {
	static SpinLock lock;
	lock.Enter();
	Cout() << s;
	Cout().PutEol();
	lock.Leave();
}


Server::Server() {
	
}

void Server::Init() {
	db.Init();
}

void Server::Listen() {
	
	if(!listener.Listen(17000, 5)) {
		Print("Unable to initialize server socket!");
		SetExitCode(1);
		return;
	}
	Print("Waiting for requests..");
	TimeStop ts;
	while (!Thread::IsShutdownThreads()) {
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
				
				lock.EnterWrite();
				sessions.Add(id, ses.Detach()).Start();
				lock.LeaveWrite();
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
		}
	}
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
	SendMessage(user.user_id, "join " + user.name + " " + IntStr(user.user_id) + " " + channel, ch.users);
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
	SendMessage(user.user_id, "leave " + user.name + " " + IntStr(user.user_id) + " " + channel, ch.users);
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

MessageRef& Server::GetReference(unsigned hash) {
	msglock.EnterRead();
	int i = messages.Find(hash);
	if (i == -1) {msglock.LeaveRead(); throw Exc("Message removed");}
	MessageRef& ref = messages[i];
	msglock.LeaveRead();
	return ref;
}

void Server::DecReference(MessageRef& ref) {
	ref.refcount--;
	if (ref.refcount == 0) {
		msglock.EnterWrite();
		if (ref.refcount == 0) // check again after acquiring the lock
			messages.RemoveKey(ref.hash);
		msglock.LeaveWrite();
	}
}








ActiveSession::ActiveSession() {
	
}

void ActiveSession::GetUserlist(Index<int>& userlist) {
	for(int i = 0; i < channels.GetCount(); i++) {
		const Channel& ch = server->channels.Get(channels[i]);
		for(int j = 0; j < ch.users.GetCount(); j++)
			userlist.FindAdd(ch.users[j]);
	}
}

void ActiveSession::Run() {
	Print("Session " + IntStr(sess_id) + " started handling socket from " + s.GetPeerAddr());
	try {
		
		while (s.IsOpen()) {
			int r;
			int in_size;
			r = s.Get(&in_size, sizeof(in_size));
			if (r != sizeof(in_size) || in_size < 0 || in_size >= 100000) throw Exc("Received invalid size");
			
			String in_data = s.Get(in_size);
			if (in_data.GetCount() != in_size) throw Exc("Received invalid data");
			
			AESDecoderStream dec("passw0rdpassw0rd");
			dec.AddData(in_data);
			String dec_data = dec.GetDecryptedData();
			MemReadStream in(dec_data.Begin(), dec_data.GetCount());
			StringStream out;
			
			int code;
			r = in.Get(&code, sizeof(code));
			if (r != sizeof(code)) throw Exc("Received invalid code");
			
			if (code > 200 && user_id == -1) throw Exc("Not logged in");
			
			switch (code) {
				case 100:		Register(in, out); break;
				case 200:		Login(in, out); break;
				case 300:		Set(in, out); break;
				case 400:		Get(in, out); break;
				case 500:		Join(in, out); break;
				case 600:		Leave(in, out); break;
				case 700:		Message(in, out); break;
				case 800:		Poll(in, out); break;
				case 900:		Location(in, out); break;
				case 1000:		ChannelMessage(in, out); break;
				
				default:
					throw Exc("Received invalid code " + IntStr(code));
			}
			
			AESEncoderStream enc(10000, "passw0rdpassw0rd");
			out.Seek(0);
			String out_str = out.Get(out.GetSize());
			if (out_str.GetCount() % AES_BLOCK_SIZE != 0)
				out_str.Cat(0, AES_BLOCK_SIZE - (out_str.GetCount() % AES_BLOCK_SIZE));
			enc.AddData(out_str);
			String out_data = enc.GetEncryptedData();
			int out_size = out_data.GetCount();
			
			r = s.Put(&out_size, sizeof(out_size));
			if (r != sizeof(out_size)) throw Exc("Data sending failed");
			r = s.Put(out_data.Begin(), out_data.GetCount());
			if (r != out_data.GetCount()) throw Exc("Data sending failed");
		}
	}
	catch (Exc e) {
		Print("Error processing client from: " + s.GetPeerAddr() + " Reason: " + e);
	}
	catch (const char* e) {
		Print("Error processing client from: " + s.GetPeerAddr() + " Reason: " + e);
	}
	catch (...) {
		Print("Error processing client from: " + s.GetPeerAddr());
	}
	
	Logout();
	
	s.Close();
}

void ActiveSession::Register(Stream& in, Stream& out) {
	if (db.IsOpen()) throw Exc("Invalid protocol");
	
	server->lock.EnterWrite();
	
	int id = server->db.GetUserCount();
	String nick = "User" + IntStr(id);
	
	String pass = RandomPassword(8);
	int64 passhash = pass.GetHashValue();
	Time now = GetUtcTime();
	
	server->db.AddUser(nick);
	
	if (db.Init(id)) {
		server->lock.LeaveWrite();
		throw Exc("DataBase init failed");
	}
	
	db.SetStr(NICK, nick);
	db.SetInt(PASSHASH, passhash);
	db.SetTime(JOINED, now);
	db.SetTime(LASTLOGIN, Time(1970,1,1));
	db.SetInt(LOGINS, 0);
	db.SetInt(ONLINETOTAL, 0);
	db.SetInt(VISIBLETOTAL, 0);
	
	db.SetDbl(LL_LON, 0);
	db.SetDbl(LL_LAT, 0);
	db.SetDbl(LL_ELEVATION, 0);
	db.SetTime(LL_UPDATED, Time(1970,1,1));
	
	int ch_count0 = db.GetChannelCount();
	db.AddChannel("oulu");
	db.AddChannel("news");
	db.AddChannel("testers");
	int ch_count1 = db.GetChannelCount();
	
	db.Flush();
	
	server->lock.LeaveWrite();
	
	out.Put(&id, sizeof(id));
	out.Put(pass.Begin(), 8);
	
	Print("Registered " + IntStr(id) + " " + pass + " (hash " + IntStr64(passhash) + ")");
}

void ActiveSession::Login(Stream& in, Stream& out) {
	int r = in.Get(&user_id, sizeof(user_id));
	if (r != sizeof(user_id)) throw Exc("Invalid login id");
	
	String pass = in.Get(8);
	if (pass.GetCount() != 8) throw Exc("Invalid login password");
	int64 passhash = pass.GetHashValue();
	
	if (!db.IsOpen()) {
		if (db.Init(user_id))
			throw Exc("Database init failed");
	}
	
	int64 correct_passhash = db.GetInt(PASSHASH);
	if (passhash != correct_passhash || !correct_passhash) throw Exc("Invalid login password");
	
	int logins = db.GetInt(LOGINS);
	
	server->lock.EnterWrite();
	server->user_session_ids.GetAdd(user_id, sess_id) = sess_id;
	server->lock.LeaveWrite();
	
	name = db.GetString(NICK);
	
	db.SetInt(LOGINS, logins + 1);
	
	int ch_count = db.GetChannelCount();
	for(int i = 0; i < ch_count; i++) {
		String channel = db.GetChannel(i);
		if (!channel.IsEmpty())
			server->JoinChannel(channel, *this);
	}
	
	out.Put32(0);
	
	out.Put32(name.GetCount());
	out.Put(name.Begin(), name.GetCount());
}

void ActiveSession::Logout() {
	server->lock.EnterWrite();
	server->user_session_ids.RemoveKey(user_id);
	server->lock.LeaveWrite();
	
	while (!channels.IsEmpty())
		server->LeaveChannel(server->channels.Get(channels[0]).name, *this);
	
	// Dereference messages
	while (!inbox.IsEmpty()) {
		try {
			lock.Enter();
			if (inbox.IsEmpty()) {lock.Leave(); break;}
			InboxMessage msg = inbox.Pop();
			lock.Leave();
			
			MessageRef& ref = server->GetReference(msg.msg);
			server->DecReference(ref);
		}
		catch (Exc e) {
			Print("Messages deleted");
		}
	}
	
	db.Deinit();
}

void ActiveSession::Set(Stream& in, Stream& out) {
	int r;
	int key_len;
	String key;
	r = in.Get(&key_len, sizeof(key_len));
	if (r != sizeof(key_len) || key_len < 0 || key_len > 200) throw Exc("Invalid key argument");
	key = in.Get(key_len);
	if (key.GetCount() != key_len) throw Exc("Invalid key received");
	
	int value_len;
	String value;
	r = in.Get(&value_len, sizeof(value_len));
	if (r != sizeof(value_len) || value_len < 0 || value_len > 10000) throw Exc("Invalid value argument");
	value = in.Get(value_len);
	if (value.GetCount() != value_len) throw Exc("Invalid value received");
	
	int ret = 0;
	if (key == "name") {
		int found_user_id = server->db.FindUser(value);
		if (found_user_id < 0) {
			server->db.SetUser(found_user_id, value);
			db.SetStr(NICK, value);
			db.Flush();
			name = value;

			Index<int> userlist;
			GetUserlist(userlist);
			userlist.RemoveKey(user_id);
			server->lock.EnterRead();
			server->SendMessage(user_id, "name " + IntStr(user_id) + " " + value, userlist);
			server->lock.LeaveRead();
		} else {
			ret = 1;
		}
	}
	
	out.Put32(ret);
}

void ActiveSession::Get(Stream& in, Stream& out) {
	int r;
	int key_len;
	String key;
	r = in.Get(&key_len, sizeof(key_len));
	if (r != sizeof(key_len) || key_len < 0 || key_len > 200) throw Exc("Invalid key argument");
	key = in.Get(key_len);
	if (key.GetCount() != key_len) throw Exc("Invalid key received");
	
	int64 size_pos = out.GetPos();
	out.SeekCur(sizeof(int));
	
	if (key == "channellist") {
		server->lock.EnterRead();
		
		out.Put32(channels.GetCount());
		for(int i = 0; i < channels.GetCount(); i++) {
			const Channel& ch = server->channels.Get(channels[i]);
			out.Put32(ch.name.GetCount());
			out.Put(ch.name.Begin(), ch.name.GetCount());
		}
		
		server->lock.LeaveRead();
	}
	else if (key == "userlist") {
		server->lock.EnterRead();
		
		Index<int> userlist;
		GetUserlist(userlist);
		userlist.RemoveKey(user_id);
		out.Put32(userlist.GetCount());
		for(int i = 0; i < userlist.GetCount(); i++) {
			int j = server->user_session_ids.Get(userlist[i]);
			const ActiveSession& as = server->sessions.Get(j);
			out.Put32(as.user_id);
			out.Put32(as.name.GetCount());
			if (!as.name.IsEmpty())
				out.Put(as.name.Begin(), as.name.GetCount());
			
			out.Put32(as.channels.GetCount());
			for(int j = 0; j < as.channels.GetCount(); j++) {
				const Channel& ch = server->channels.Get(as.channels[j]);
				out.Put32(ch.name.GetCount());
				out.Put(ch.name.Begin(), ch.name.GetCount());
			}
		}
		
		server->lock.LeaveRead();
	}
	
	out.Seek(size_pos);
	out.Put32(out.GetSize() - size_pos - 4);
	out.SeekEnd();
	out.Put32(0);
}

void ActiveSession::Join(Stream& in, Stream& out) {
	int ch_len;
	in.Get(&ch_len, sizeof(ch_len));
	if (ch_len < 0 || ch_len > 200) throw Exc("Invalid channel name");
	String ch_name = in.Get(ch_len);
	if (ch_name.GetCount() != ch_len) throw Exc("Invalid channel name");
	
	// Check if user is already joined at the channel
	int i = db.FindChannel(ch_name);
	if (i >= 0) {
		out.Put32(1);
	} else {
		db.AddChannel(ch_name);
		
		server->JoinChannel(ch_name, *this);
		
		out.Put32(0);
	}
}

void ActiveSession::Leave(Stream& in, Stream& out) {
	int ch_len;
	in.Get(&ch_len, sizeof(ch_len));
	if (ch_len < 0 || ch_len > 200) throw Exc("Invalid channel name");
	String ch_name = in.Get(ch_len);
	if (ch_name.GetCount() != ch_len) throw Exc("Invalid channel name");
	
	int i = db.FindChannel(ch_name);
	if (i >= 0)
		db.DeleteChannel(i);
	server->LeaveChannel(ch_name, *this);
	
	out.Put32(0);
}

void ActiveSession::Location(Stream& in, Stream& out) {
	int r;
	double lat, lon, elev;
	r = in.Get(&lat, sizeof(lat));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	r = in.Get(&lon, sizeof(lon));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	r = in.Get(&elev, sizeof(elev));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	
	db.SetLocation(lon, lat, elev);
	
	out.Put32(0);
}

void ActiveSession::Message(Stream& in, Stream& out) {
	int r, i;
	int recv_user_id, recv_msg_len;
	String recv_msg;
	r = in.Get(&recv_user_id, sizeof(recv_user_id));
	if (r != sizeof(recv_user_id)) throw Exc("Invalid message argument");
	r = in.Get(&recv_msg_len, sizeof(recv_msg_len));
	if (r != sizeof(recv_msg_len) || recv_msg_len < 0 || recv_msg_len > 10000) throw Exc("Invalid message argument");
	recv_msg = in.Get(recv_msg_len);
	if (recv_msg.GetCount() != recv_msg_len) throw Exc("Invalid message received");
	
	server->lock.EnterRead();
	i = server->user_session_ids.Find(recv_user_id);
	if (i < 0)  {server->lock.LeaveRead(); out.Put32(1); return;}
	int recv_sess_id = server->user_session_ids[i];
	i = server->sessions.Find(recv_sess_id);
	if (i < 0)  {server->lock.LeaveRead(); throw Exc("System error");}
	ActiveSession& recv_as = server->sessions[i];
	server->lock.LeaveRead();
	
	const MessageRef& ref = server->IncReference("msg " + recv_msg, 1);
	
	lock.Enter();
	InboxMessage& msg = recv_as.inbox.Add();
	msg.msg = ref.hash;
	msg.sender_id = user_id;
	lock.Leave();
	
	out.Put32(0);
}

void ActiveSession::Poll(Stream& in, Stream& out) {
	lock.Enter();
	int count = inbox.GetCount();
	out.Put32(count);
	for(int i = 0; i < count; i++) {
		const InboxMessage& im = inbox[i];
		out.Put32(im.sender_id);
		
		MessageRef& ref = server->GetReference(im.msg);
		int msg_len = ref.msg.GetCount();
		out.Put32(msg_len);
		if (msg_len > 0)
			out.Put(ref.msg.Begin(), msg_len);
		
		server->DecReference(ref);
	}
	inbox.SetCount(0);
	lock.Leave();
}

void ActiveSession::ChannelMessage(Stream& in, Stream& out) {
	int r, i;
	int recv_ch_len, recv_msg_len;
	String recv_ch, recv_msg;
	r = in.Get(&recv_ch_len, sizeof(recv_ch_len));
	if (r != sizeof(recv_ch_len) || recv_ch_len > 10000) throw Exc("Invalid message argument");
	recv_ch = in.Get(recv_ch_len);
	if (recv_ch.GetCount() != recv_ch_len) throw Exc("Invalid message argument");
	r = in.Get(&recv_msg_len, sizeof(recv_msg_len));
	if (r != sizeof(recv_msg_len) || recv_msg_len < 0 || recv_msg_len > 10000) throw Exc("Invalid message argument");
	recv_msg = in.Get(recv_msg_len);
	if (recv_msg.GetCount() != recv_msg_len) throw Exc("Invalid message received");
	
	server->lock.EnterRead();
	i = server->channel_ids.Find(recv_ch);
	if (i < 0)  {server->lock.LeaveRead(); out.Put32(1); return;}
	int recv_sess_id = server->channel_ids[i];
	i = server->channels.Find(recv_sess_id);
	if (i < 0)  {server->lock.LeaveRead(); throw Exc("System error");}
	Channel& ch = server->channels[i];
	server->lock.LeaveRead();
	
	const MessageRef& ref = server->IncReference("chmsg " + recv_ch + " " + recv_msg, 1);
	
	lock.Enter();
	for(int i = 0; i < ch.users.GetCount(); i++) {
		int id = ch.users[i];
		if (id == user_id) continue;
		int j = server->user_session_ids.Find(id);
		if (j == -1) continue;
		id = server->user_session_ids[j];
		j = server->sessions.Find(id);
		if (j == -1) continue;
		ActiveSession& recv_as = server->sessions[j];
		InboxMessage& msg = recv_as.inbox.Add();
		msg.msg = ref.hash;
		msg.sender_id = user_id;
	}
	lock.Leave();
	
	out.Put32(0);
}




CONSOLE_APP_MAIN {
	try {
		Server s;
		s.Init();
		s.Listen();
	}
	catch (Exc e) {
		Print("Error. Reason: " + e);
	}
}
