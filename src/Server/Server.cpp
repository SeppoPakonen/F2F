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


Server::Server() {
	
}

void Server::Init() {
	sqlite3.LogErrors(true);
	if(!sqlite3.Open(ConfigFile("server.db"))) {
		throw Exc("Can't create or open database file");
	}

	SQL = sqlite3;
	
	// Update the schema to match the schema described in "db.sch"
	#ifdef _DEBUG
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
	#endif
	
	sql.Create();
}

void Server::Listen() {
	
	if(!listener.Listen(17000, 5)) {
		Cout() << "Unable to initialize server socket!\n";
		SetExitCode(1);
		return;
	}
	Cout() << "Waiting for requests..\n";
	while (!Thread::IsShutdownThreads()) {
		One<TcpSocket> s;
		s.Create();
		if(s->Accept(listener)) {
			Thread::Start(THISBACK1(HandleSocket, s.Detach()));
		}
	}
}

void Server::HandleSocket(One<TcpSocket> s) {
	Cout() << "Started handling socket from " << s->GetPeerAddr() << "\n";
	try {
		
		// Close blacklisted connections
		if (blacklist.Find(s->GetPeerAddr()) != -1)
			throw Exc("Address is blacklisted");
		
		while (s->IsOpen()) {
			int r;
			int in_size;
			r = s->Get(&in_size, sizeof(in_size));
			if (r != sizeof(in_size) || in_size < 0 || in_size >= 100000) throw Exc("Received invalid size");
			
			String in_data = s->Get(in_size);
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
			
			r = s->Put(&out_size, sizeof(out_size));
			if (r != sizeof(out_size)) throw Exc("Data sending failed");
			r = s->Put(out_data.Begin(), out_data.GetCount());
			if (r != out_data.GetCount()) throw Exc("Data sending failed");
			
			Cout() << "Sent " << out_str.GetCount() << " (" << out_size << ")\n";
		}
	}
	catch (Exc e) {
		Cout() << "Error processing client from: " << s->GetPeerAddr() << " Reason: " << e << '\n';
	}
	catch (const char* e) {
		Cout() << "Error processing client from: " << s->GetPeerAddr() << " Reason: " << e << '\n';
	}
	catch (...) {
		Cout() << "Error processing client from: " << s->GetPeerAddr() << '\n';
	}
	
	s->Close();
}

void Server::Register(Stream& in, Stream& out) {
	Sql& sql = *this->sql;
	
	lock.Enter();
	
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
	
	lock.Leave();
	
	out.Put(&id, sizeof(id));
	out.Put(pass.Begin(), 8);
	
	Cout() << "Registered " << id << " " << pass << " (hash " << passhash << ")\n";
}

void Server::Login(Stream& in, Stream& out) {
	Sql& sql = *this->sql;
	int id;
	
	int r = in.Get(&id, sizeof(id));
	if (r != sizeof(id)) throw Exc("Invalid login id");
	
	String pass = in.Get(8);
	if (pass.GetCount() != 8) throw Exc("Invalid login password");
	int64 passhash = pass.GetHashValue();
	
	lock.Enter();
	
	sql*Select(PASSHASH).From(USERS).Where(ID == id);
	bool found = sql.Fetch();
	if (sql.WasError() || !found) {lock.Leave(); throw Exc("Invalid login password");}
	int64 correct_passhash = sql[0];
	if (passhash != correct_passhash) {lock.Leave(); throw Exc("Invalid login password");}
	
	String sesspass = RandomPassword(8);
	
	sql*Select(LOGINS).From(USERS).Where(ID == id);
	sql.Fetch();
	int logins = sql[0];
	
	ActiveSession& as = sessions.Add(sesspass);
	as.user_id = id;
	user_session_ids.GetAdd(id) = sesspass;
	
	sql*Select(NICK).From(USERS).Where(ID == id);
	sql.Fetch();
	as.name = sql[0];
	
	sql*Update(USERS)(LASTLOGIN, GetUtcTime())(LOGINS, logins + 1).Where(ID == id);
	
	lock.Leave();
	
	out.Put(sesspass.Begin(), 8);
}

void Server::Set(Stream& in, Stream& out) {
	Sql& sql = *this->sql;
	int r;
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	
	int key_len;
	String key;
	r = in.Get(&key_len, sizeof(key_len));
	if (r != sizeof(key_len) || key_len < 0 || key_len > 200) {lock.Leave(); throw Exc("Invalid key argument");}
	key = in.Get(key_len);
	if (key.GetCount() != key_len) {lock.Leave(); throw Exc("Invalid key received");}
	
	int value_len;
	String value;
	r = in.Get(&value_len, sizeof(value_len));
	if (r != sizeof(value_len) || value_len < 0 || value_len > 10000) {lock.Leave(); throw Exc("Invalid value argument");}
	value = in.Get(value_len);
	if (value.GetCount() != value_len) {lock.Leave(); throw Exc("Invalid value received");}
	
	int ret = 0;
	if (key == "name") {
		sql*Select(Count(ID)).From(USERS).Where(NICK == value);
		int exists = sql[0];
		if (!exists) {
			sql*Update(USERS)(NICK, value).Where(ID == as.user_id);
			as.name = value;
		} else {
			ret = 1;
		}
		//TODO send name change to all
	}
	
	lock.Leave();
	
	out.Put32(ret);
}

void Server::Get(Stream& in, Stream& out) {
	Sql& sql = *this->sql;
	int r;
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	
	int key_len;
	String key;
	r = in.Get(&key_len, sizeof(key_len));
	if (r != sizeof(key_len) || key_len < 0 || key_len > 200) {lock.Leave(); throw Exc("Invalid key argument");}
	key = in.Get(key_len);
	if (key.GetCount() != key_len) {lock.Leave(); throw Exc("Invalid key received");}
	
	int64 size_pos = out.GetPos();
	out.SeekCur(sizeof(int));
	
	if (key == "userlist") {
		out.Put32(sessions.GetCount());
		for(int i = 0; i < sessions.GetCount(); i++) {
			const ActiveSession& as = sessions[i];
			out.Put32(as.user_id);
			out.Put32(as.name.GetCount());
			if (!as.name.IsEmpty())
				out.Put(as.name.Begin(), as.name.GetCount());
		}
	}
	
	lock.Leave();
	
	out.Seek(size_pos);
	out.Put32(out.GetSize() - size_pos - 4);
	out.SeekEnd();
	out.Put32(0);
}

void Server::Join(Stream& in, Stream& out) {
	Sql& sql = *this->sql;
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	
	int ch_len;
	in.Get(&ch_len, sizeof(ch_len));
	if (ch_len < 0 || ch_len > 200) {lock.Leave(); throw Exc("Invalid channel name");}
	String ch_name = in.Get(ch_len);
	if (ch_name.GetCount() != ch_len) {lock.Leave(); throw Exc("Invalid channel name");}
	
	sql*Insert(CHANNELS)(CH_USER_ID, as.user_id)(CHANNEL, ch_name)(CH_JOINED, GetUtcTime());
	
	lock.Leave();
	
	out.Put32(0);
}

void Server::Leave(Stream& in, Stream& out) {
	Sql& sql = *this->sql;
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	
	int ch_len;
	in.Get(&ch_len, sizeof(ch_len));
	if (ch_len < 0 || ch_len > 200) {lock.Leave(); throw Exc("Invalid channel name");}
	String ch_name = in.Get(ch_len);
	if (ch_name.GetCount() != ch_len) {lock.Leave(); throw Exc("Invalid channel name");}
	
	sql*Delete(CHANNELS).Where(CH_USER_ID == as.user_id && CHANNEL == ch_name);
	
	lock.Leave();
	
	out.Put32(0);
}

void Server::Location(Stream& in, Stream& out) {
	Sql& sql = *this->sql;
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	
	int r;
	double lat, lon, elev;
	r = in.Get(&lat, sizeof(lat));
	if (r != sizeof(double)) {lock.Leave(); throw Exc("Invalid location argument");}
	r = in.Get(&lon, sizeof(lon));
	if (r != sizeof(double)) {lock.Leave(); throw Exc("Invalid location argument");}
	r = in.Get(&elev, sizeof(elev));
	if (r != sizeof(double)) {lock.Leave(); throw Exc("Invalid location argument");}
	
	sql*Update(LAST_LOCATION)(LL_LON, lon)(LL_LAT, lat)(LL_ELEVATION, elev)(LL_UPDATED, GetUtcTime()).Where(LL_USER_ID == as.user_id);
	
	sql*Insert(LOCATION_HISTORY)(LH_USER_ID, as.user_id)(LH_LON, lon)(LH_LAT, lat)(LH_ELEVATION, elev)(LH_UPDATED, GetUtcTime());
	
	out.Put32(0);
	
	lock.Leave();
}

void Server::Message(Stream& in, Stream& out) {
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	
	int r;
	int recv_user_id, recv_msg_len;
	String recv_msg;
	r = in.Get(&recv_user_id, sizeof(recv_user_id));
	if (r != sizeof(recv_user_id)) {lock.Leave(); throw Exc("Invalid message argument");}
	r = in.Get(&recv_msg_len, sizeof(recv_msg_len));
	if (r != sizeof(recv_msg_len) || recv_msg_len < 0 || recv_msg_len > 10000) {lock.Leave(); throw Exc("Invalid message argument");}
	recv_msg = in.Get(recv_msg_len);
	if (recv_msg.GetCount() != recv_msg_len) {lock.Leave(); throw Exc("Invalid message received");}
	
	i = user_session_ids.Find(recv_user_id);
	if (i < 0)  {lock.Leave(); throw Exc("Invalid receiver id");}
	String recv_sesspass = user_session_ids[i];
	i = sessions.Find(recv_sesspass);
	if (i < 0)  {lock.Leave(); throw Exc("System error");}
	ActiveSession& recv_as = sessions[i];
	
	lock.Leave();
	
	recv_as.lock.Enter();
	InboxMessage& msg = recv_as.inbox.Add();
	msg.message = recv_msg;
	msg.sender_id = as.user_id;
	recv_as.lock.Leave();
	
	out.Put32(0);
}

void Server::Poll(Stream& in, Stream& out) {
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	
	lock.Leave();
	
	as.lock.Enter();
	int count = as.inbox.GetCount();
	out.Put32(count);
	for(int i = 0; i < count; i++) {
		InboxMessage& im = as.inbox[i];
		out.Put32(im.sender_id);
		
		int msg_len = im.message.GetCount();
		out.Put32(msg_len);
		out.Put(im.message.Begin(), msg_len);
	}
	as.inbox.SetCount(0);
	as.lock.Leave();
}



CONSOLE_APP_MAIN {
	try {
		Server s;
		s.Init();
		s.Listen();
	}
	catch (Exc e) {
		Cout() << "Error. Reason: " << e << '\n';
	}
}
