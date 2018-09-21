#include "Server.h"
#include "AES.h"

#define MODEL <Server/db.sch>
#define SCHEMADIALECT <plugin/sqlite3/Sqlite3Schema.h>
#include <Sql/sch_header.h>
#include <Sql/sch_schema.h>
#include <Sql/sch_source.h>


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
	sqlite3.LogErrors(true);
	if(!sqlite3.Open(ConfigFile("server.db"))) {
		throw Exc("Can't create or open database file");
	}

	SQL = sqlite3;
	
	// Update the schema to match the schema described in "db.sch"
	SqlSchema sch(SQLITE3);
	All_Tables(sch);
	if(sch.ScriptChanged(SqlSchema::UPGRADE))
		SqlPerformScript(sch.Upgrade());
	if(sch.ScriptChanged(SqlSchema::ATTRIBUTES)) {
		SqlPerformScript(sch.Attributes());
	}
	if(sch.ScriptChanged(SqlSchema::CONFIG)) {
		SqlPerformScript(sch.ConfigDrop());
		SqlPerformScript(sch.Config());
	}
	sch.SaveNormal();
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
			ses->s.Timeout(5000);
			
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
	ch.users.Add(user.user_id);
	user.channels.Add(id);
	lock.LeaveWrite();
}

void Server::LeaveChannel(const String& channel, ActiveSession& user) {
	lock.EnterWrite();
	int id = channel_ids.Get(channel);
	Channel& ch = channels.Get(id);
	ch.users.RemoveKey(user.user_id);
	user.channels.RemoveKey(id);
	SendMessage(user.user_id, "leave " + user.name + " " + IntStr(user.user_id) + " " + channel, ch.users);
	lock.LeaveWrite();
}

void Server::SendMessage(int sender_id, const String& msg, const Index<int>& user_list) {
	if (msg.IsEmpty()) return;
	const MessageRef& ref = IncReference(msg, user_list.GetCount());
	for(int i = 0; i < user_list.GetCount(); i++) {
		ActiveSession& as = sessions.Get(user_session_ids.Get(user_list[i]));
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
	MessageRef& ref = messages.Get(hash);
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
	
	s.Close();
	
	Logout();
}

void ActiveSession::Register(Stream& in, Stream& out) {
	
	server->lock.EnterWrite();
	
	int id = 0;
	sql*Select(Count(ID)).From(USERS);
	if (!sql.WasError()) {
		id = sql[0];
	}
	String nick = "User" + IntStr(id);
	
	String pass = RandomPassword(8);
	int64 passhash = pass.GetHashValue();
	Time now = GetUtcTime();
	
	sql*Insert(USERS)
		(ID, id)
		(NICK, nick)
		(PASSHASH, passhash)
		(JOINED, now)
		(LASTLOGIN, 0)
		(LOGINS, 0)
		(ONLINETOTAL, 0)
		(VISIBLETOTAL, 0);
	
	sql*Insert(LAST_LOCATION)
		(LL_USER_ID, id)
		(LL_LON, 0)
		(LL_LAT, 0)
		(LL_ELEVATION, 0)
		(LL_UPDATED, 0);
	
	sql*Insert(CHANNELS)(CH_USER_ID, id)(CHANNEL, "oulu")(CH_JOINED, GetUtcTime());
	sql*Insert(CHANNELS)(CH_USER_ID, id)(CHANNEL, "news")(CH_JOINED, GetUtcTime());
	sql*Insert(CHANNELS)(CH_USER_ID, id)(CHANNEL, "testers")(CH_JOINED, GetUtcTime());
	
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
	
	sql*Select(PASSHASH).From(USERS).Where(ID == user_id);
	bool found = sql.Fetch();
	if (sql.WasError() || !found) throw Exc("Invalid login password");
	int64 correct_passhash = sql[0];
	if (passhash != correct_passhash) throw Exc("Invalid login password");
	
	sql*Select(LOGINS).From(USERS).Where(ID == user_id);
	sql.Fetch();
	int logins = sql[0];
	
	server->lock.EnterWrite();
	server->user_session_ids.GetAdd(user_id, sess_id) = sess_id;
	server->lock.LeaveWrite();
	
	sql*Select(NICK).From(USERS).Where(ID == user_id);
	sql.Fetch();
	name = sql[0];
	
	sql*Update(USERS)(LASTLOGIN, GetUtcTime())(LOGINS, logins + 1).Where(ID == user_id);
	
	sql*Select(CHANNEL).From(CHANNELS).Where(CH_USER_ID == user_id);
	while (sql.Fetch()) {
		String channel = sql[0];
		server->JoinChannel(channel, *this);
	}
	
	out.Put32(0);
}

void ActiveSession::Logout() {
	server->lock.EnterWrite();
	server->user_session_ids.RemoveKey(user_id);
	server->lock.LeaveWrite();
	
	while (!channels.IsEmpty())
		server->LeaveChannel(server->channels.Get(channels[0]).name, *this);
	
	// Dereference messages
	for(int i = 0; i < inbox.GetCount(); i++) {
		InboxMessage& msg = inbox[i];
		MessageRef& ref = server->GetReference(msg.msg);
		server->DecReference(ref);
	}
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
		sql*Select(Count(ID)).From(USERS).Where(NICK == value);
		int exists = sql[0];
		if (!exists) {
			sql*Update(USERS)(NICK, value).Where(ID == user_id);
			name = value;
		} else {
			ret = 1;
		}
		
		Index<int> userlist;
		GetUserlist(userlist);
		server->SendMessage(user_id, "name " + IntStr(user_id) + " " + value, userlist);
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
	
	if (key == "userlist") {
		server->lock.EnterRead();
		
		Index<int> userlist;
		GetUserlist(userlist);
		out.Put32(userlist.GetCount());
		for(int i = 0; i < userlist.GetCount(); i++) {
			int j = server->user_session_ids.Get(userlist[i]);
			const ActiveSession& as = server->sessions[j];
			out.Put32(as.user_id);
			out.Put32(as.name.GetCount());
			if (!as.name.IsEmpty())
				out.Put(as.name.Begin(), as.name.GetCount());
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
	sql*Select(CH_USER_ID).From(CHANNELS).Where(CH_USER_ID == user_id && CHANNEL == ch_name);
	if (sql.Fetch()) {
		out.Put32(1);
	} else {
		sql*Insert(CHANNELS)(CH_USER_ID, user_id)(CHANNEL, ch_name)(CH_JOINED, GetUtcTime());
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
	
	sql*Delete(CHANNELS).Where(CH_USER_ID == user_id && CHANNEL == ch_name);
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
	
	sql*Update(LAST_LOCATION)(LL_LON, lon)(LL_LAT, lat)(LL_ELEVATION, elev)(LL_UPDATED, GetUtcTime()).Where(LL_USER_ID == user_id);
	
	sql*Insert(LOCATION_HISTORY)(LH_USER_ID, user_id)(LH_LON, lon)(LH_LAT, lat)(LH_ELEVATION, elev)(LH_UPDATED, GetUtcTime());
	
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
	if (i < 0)  {server->lock.LeaveRead(); throw Exc("Invalid receiver id");}
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
		out.Put(ref.msg.Begin(), msg_len);
		
		server->DecReference(ref);
	}
	inbox.SetCount(0);
	lock.Leave();
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
