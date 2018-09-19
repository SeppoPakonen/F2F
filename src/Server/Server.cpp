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
}

void Server::Listen() {
	
	if(!listener.Listen(3214, 5)) {
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
	try {
		
		// Close blacklisted connections
		if (blacklist.Find(s->GetPeerAddr()) != -1)
			throw Exc("Address is blacklisted");
		
		while (s->IsOpen()) {
			int r;
			int size;
			r = s->Get(&size, sizeof(size));
			if (r != sizeof(size) || size < 0 || size >= 100000) throw Exc("Received invalid size");
			
			String data = s->Get(size);
			if (data.GetCount() != size) throw Exc("Received invalid data");
			
			AESDecoderStream dec("passw0rd");
			dec.AddData(data);
			data = dec.GetDecryptedData();
			MemReadStream in(data.Begin(), data.GetCount());
			StringStream out;
			
			int code;
			r = in.Get(&code, sizeof(code));
			if (r != sizeof(code)) throw Exc("Received invalid code");
			
			switch (code) {
				case 100:		Register(in, out); break;
				case 200:		Login(in, out); break;
				case 300:		Join(in, out); break;
				case 400:		Leave(in, out); break;
				case 500:		Location(in, out); break;
				case 600:		Message(in, out); break;
				
				// TODO: NAME(), POLL()
				default:
					throw Exc("Received invalid code " + IntStr(code));
			}
		}
	}
	catch (Exc e) {
		Cout() << "Error processing client from: " << s->GetPeerAddr() << " Reason: " << e << '\n';
	}
	catch (...) {
		Cout() << "Error processing client from: " << s->GetPeerAddr() << '\n';
	}
	
	s->Close();
}

void Server::Register(Stream& in, Stream& out) {
	lock.Enter();
	int id = Select(Count(ID)).From(USERS);
	id++;
	
	String pass = RandomPassword(8);
	int passhash = pass.GetHashValue();
	Time now = GetUtcTime();
	
	Insert(USERS)
		(ID, id)
		(NICK, "Unnamed")
		(PASSHASH, passhash)
		(JOINED, now)
		(LASTLOGIN, 0)
		(LOGINS, 0)
		(ONLINETOTAL, 0)
		(VISIBLETOTAL, 0);
	
	Insert(LAST_LOCATION)
		(LL_USER_ID, id)
		(LL_LON, 0)
		(LL_LAT, 0)
		(LL_ELEVATION, 0)
		(LL_UPDATED, 0);
	
	lock.Leave();
	
	out.Put(&id, sizeof(id));
	out.Put(pass.Begin(), 8);
}

void Server::Login(Stream& in, Stream& out) {
	int id;
	
	int r = in.Get(&id, sizeof(id));
	if (r != sizeof(id)) throw Exc("Invalid login id");
	
	String pass = in.Get(8);
	if (pass.GetCount() != 8) throw Exc("Invalid login password");
	int passhash = pass.GetHashValue();
	
	int correct_passhash = Select(PASSHASH).From(USERS).Where(ID == id);
	if (passhash != correct_passhash) throw Exc("Invalid login password");
	
	String sesspass = RandomPassword(8);
	
	int logins = Select(LOGINS).From(USERS).Where(ID == id);
	
	lock.Enter();
	ActiveSession& as = sessions.Add(sesspass);
	as.user_id = id;
	user_session_ids.GetAdd(id) = sesspass;
	lock.Leave();
	
	Update(USERS)(LASTLOGIN, GetUtcTime())(LOGINS, logins + 1).Where(ID == id);
	
	out.Put(sesspass.Begin(), 8);
}

void Server::Join(Stream& in, Stream& out) {
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	lock.Leave();
	
	int ch_len;
	in.Get(&ch_len, sizeof(ch_len));
	if (ch_len < 0 || ch_len > 200) throw Exc("Invalid channel name");
	String ch_name = in.Get(ch_len);
	if (ch_name.GetCount() != ch_len) throw Exc("Invalid channel name");
	
	Insert(CHANNELS)(CH_USER_ID, as.user_id)(CHANNEL, ch_name)(JOINED, GetUtcTime());
	
	out.Put32(0);
}

void Server::Leave(Stream& in, Stream& out) {
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	lock.Leave();
	
	int ch_len;
	in.Get(&ch_len, sizeof(ch_len));
	if (ch_len < 0 || ch_len > 200) throw Exc("Invalid channel name");
	String ch_name = in.Get(ch_len);
	if (ch_name.GetCount() != ch_len) throw Exc("Invalid channel name");
	
	Delete(CHANNELS).Where(CH_USER_ID == as.user_id && CHANNEL == ch_name);
	
	out.Put32(0);
}

void Server::Location(Stream& in, Stream& out) {
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	lock.Leave();
	
	int r;
	double lon, lat, elev;
	r = in.Get(&lon, sizeof(lon));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	r = in.Get(&lat, sizeof(lat));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	r = in.Get(&elev, sizeof(elev));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	
	Update(LAST_LOCATION)(LL_LON, lon)(LL_LAT, lat)(LL_ELEVATION, elev)(LL_UPDATED, GetUtcTime()).Where(LL_USER_ID == as.user_id);
	
	Insert(LOCATION_HISTORY)(LH_USER_ID, as.user_id)(LH_LON, lon)(LH_LAT, lat)(LH_ELEVATION, elev)(LH_UPDATED, GetUtcTime());
	
	out.Put32(0);
}

void Server::Message(Stream& in, Stream& out) {
	String sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	lock.Enter();
	int i = sessions.Find(sesspass);
	if (i < 0) {lock.Leave(); throw Exc("Invalid session password");}
	ActiveSession& as = sessions[i];
	lock.Leave();
	
	int r;
	int recv_user_id, recv_msg_len;
	String recv_msg;
	r = in.Get(&recv_user_id, sizeof(recv_user_id));
	if (r != sizeof(recv_user_id)) throw Exc("Invalid message argument");
	r = in.Get(&recv_msg_len, sizeof(recv_msg_len));
	if (r != sizeof(recv_msg_len) || recv_msg_len < 0 || recv_msg_len > 10000) throw Exc("Invalid message argument");
	recv_msg = in.Get(recv_msg_len);
	if (recv_msg.GetCount() != recv_msg_len) throw Exc("Invalid message received");
	
	lock.Enter();
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
