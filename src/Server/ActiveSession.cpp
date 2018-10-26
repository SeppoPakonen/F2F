#include "Server.h"


ActiveSession::ActiveSession() {
	
}

void ActiveSession::Print(const String& s) {
	if (last_user_id < 0 || last_login_id == 0) return;
	UserDatabase& db = GetDatabase(last_user_id);
	db.lock.Enter();
	UserSessionLog& ses = db.sessions.GetAdd(last_login_id);
	db.lock.Leave();
	if (ses.begin == Time(1970,1,1)) ses.begin = GetSysTime();
	ses.log.Add().Set(s);
}

void ActiveSession::Run() {
	Print("Session " + IntStr(sess_id) + " started handling socket from " + s.GetPeerAddr());
	int count = 0;
	
	struct NoPrintExc : public String {NoPrintExc(String s) : String(s) {}};
	
	String hex("0123456789ABCDEF");
	try {
		
		while (s.IsOpen()) {
			int r;
			int in_size = 0;
			r = s.Get(&in_size, sizeof(in_size));
			if (r != sizeof(in_size) || in_size < 0 || in_size >= 10000000) throw NoPrintExc("Received invalid size " + IntStr(in_size));
			
			String in_data = s.Get(in_size);
			if (in_data.GetCount() != in_size) throw Exc("Received invalid data");
			
			/*String hexdump;
			for(int i = 0; i < in_data.GetCount(); i++) {
				byte b = in_data[i];
				hexdump.Cat(hex[b >> 4]);
				hexdump.Cat(hex[b & 0xF]);
			}
			LOG("in: " + hexdump);*/
			
			MemReadStream in(in_data.Begin(), in_data.GetCount());
			StringStream out;
			
			int code;
			int test = SwapEndian32(-939524096);
			r = in.Get(&code, sizeof(code));
			if (r != sizeof(code)) throw Exc("Received invalid code");
			
			
			switch (code) {
				case 0:			Greeting(in, out); break;
				case 10:		Register(in, out); break;
				case 20:		Login(in, out); break;
				case 30:		Set(in, out); break;
				case 40:		Get(in, out); break;
				case 50:		Join(in, out); break;
				case 60:		Leave(in, out); break;
				case 70:		Message(in, out); break;
				case 80:		Poll(in, out); break;
				case 90:		Location(in, out); break;
				case 100:		ChannelMessage(in, out); break;
				
				default:
					throw Exc("Received invalid code " + IntStr(code));
			}
			
			out.Seek(0);
			String out_str = out.Get(out.GetSize());
			int out_size = out_str.GetCount();
			
			/*hexdump = "";
			for(int i = 0; i < out_str.GetCount(); i++) {
				byte b = out_str[i];
				hexdump.Cat(hex[b >> 4]);
				hexdump.Cat(hex[b & 0xF]);
			}
			LOG("out " + IntStr(out_str.GetCount()) + ":" + hexdump);*/
			
			
			r = s.Put(&out_size, sizeof(out_size));
			if (r != sizeof(out_size)) throw Exc("Data sending failed");
			r = s.Put(out_str.Begin(), out_str.GetCount());
			if (r != out_str.GetCount()) throw Exc("Data sending failed");
			
			count++;
		}
	}
	catch (NoPrintExc e) {
		
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
	
	stopped = true;
}

void ActiveSession::Greeting(Stream& in, Stream& out) {
	String title = Config::server_title;
	int i = title.GetCount();
	out.Put(&i, sizeof(int));
	out.Put(title.Begin(), title.GetCount());
	i = server->sessions.GetCount();
	out.Put(&i, sizeof(int));
	i = Config::max_sessions;
	out.Put(&i, sizeof(int));
}

void ActiveSession::Register(Stream& in, Stream& out) {
	
	server->lock.EnterWrite();
	
	int user_id = server->db.GetUserCount();
	String name = "User" + IntStr(user_id);
	
	String pass = RandomPassword(8);
	int64 passhash = pass.GetHashValue();
	Time now = GetUtcTime();
	
	UserDatabase& db = GetDatabase(user_id);
	
	server->db.AddUser(user_id, name);
	server->db.Flush();
	
	db.name = name;
	db.passhash = passhash;
	db.joined = now;
	db.lastlogin = Time(1970,1,1);
	db.logins = 0;
	db.onlinetotal = 0;
	db.visibletotal = 0;
	db.longitude = 0;
	db.latitude = 0;
	db.elevation = 0;
	db.lastupdate = Time(1970,1,1);
	db.Flush();
	
	last_user_id = user_id;
	
	server->lock.LeaveWrite();
	
	server->JoinChannel("testers", user_id);
	
	out.Put(&user_id, sizeof(user_id));
	out.Put(pass.Begin(), 8);
	
	Print("Registered " + IntStr(user_id) + " " + pass + " (hash " + IntStr64(passhash) + ")");
}

void ActiveSession::Login(Stream& in, Stream& out) {
	try {
		int user_id;
		int r = in.Get(&user_id, sizeof(user_id));
		if (r != sizeof(user_id)) throw Exc("Invalid login id");
		
		if (user_id < 0 || user_id >= server->db.GetUserCount()) throw Exc("Invalid login id");
		UserDatabase& db = GetDatabase(user_id);
		last_user_id = user_id;
		
		String pass = in.Get(8);
		if (pass.GetCount() != 8) throw Exc("Invalid login password");
		int64 passhash = pass.GetHashValue();
		
		int64 correct_passhash = db.passhash;
		if (passhash != correct_passhash || !correct_passhash) throw Exc("Invalid login password");
		
		server->lock.EnterWrite();
		server->user_session_ids.GetAdd(user_id, sess_id) = sess_id;
		int64 login_id = server->GetNewLoginId();
		server->login_session_ids.GetAdd(login_id, user_id);
		server->lock.LeaveWrite();
			
		db.logins++;
		db.Flush();
		
		out.Put32(0);
		
		out.Put64(login_id);
		
		out.Put32(db.name.GetCount());
		out.Put(db.name.Begin(), db.name.GetCount());
		out.Put32(db.age);
		out.Put32(db.gender);
		
		Print("Logged in");
	}
	 catch (Exc e) {
	    out.Put32(1);
	    Print("Login failed");
	 }
}

void ActiveSession::Logout() {
	if (last_user_id < 0)
		return;
	
	server->lock.EnterWrite();
	server->user_session_ids.RemoveKey(last_user_id);
	server->lock.LeaveWrite();
	
	if (last_login_id != 0) {
		UserDatabase& db = GetDatabase(last_user_id);
		UserSessionLog& ses = db.sessions.GetAdd(last_login_id);
		ses.end = GetSysTime();
	}
}

void ActiveSession::DereferenceMessages() {
	UserDatabase& db = GetDatabase(last_user_id);
	
	while (!db.inbox.IsEmpty()) {
		try {
			db.lock.Enter();
			if (db.inbox.IsEmpty()) {db.lock.Leave(); break;}
			InboxMessage msg = db.inbox.Pop();
			db.lock.Leave();
			
			MessageRef& ref = server->GetReference(msg.msg);
			server->DecReference(ref);
		}
		catch (Exc e) {
			Print("Messages deleted");
		}
	}
}

int ActiveSession::LoginId(Stream& in) {
	int64 login_id;
	int r = in.Get(&login_id, sizeof(login_id));
	if (r != sizeof(login_id)) throw Exc("Invalid login id");
	
	server->lock.EnterRead();
	int i = server->login_session_ids.Find(login_id);
	int user_id = -1;
	if (i >= 0) user_id = server->login_session_ids[i];
	server->lock.LeaveRead();
	if (user_id < 0)
		throw Exc("Invalid login id");
	
	last_login_id = login_id;
	last_user_id = user_id;
	
	return user_id;
}

void ActiveSession::Set(Stream& in, Stream& out) {
	int user_id = LoginId(in);
	
	UserDatabase& db = GetDatabase(user_id);
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
	if (r != sizeof(value_len) || value_len < 0 || value_len > Config::max_set_string_len) throw Exc("Invalid value argument");
	value = in.Get(value_len);
	if (value.GetCount() != value_len) throw Exc("Invalid value received");
	
	int ret = 0;
	if (key == "name") {
		Print("Set name " + value);
		server->lock.EnterWrite();
		server->db.SetUser(user_id, value);
		server->db.Flush();
		server->lock.LeaveWrite();
		db.name = value;
		db.Flush();
		
		server->lock.EnterRead();
		Index<int> userlist;
		server->GetUserlist(userlist, user_id);
		userlist.RemoveKey(user_id);
		server->SendMessage(user_id, "name " + IntStr(user_id) + " " + value, userlist);
		server->lock.LeaveRead();
	}
	else if (key == "age") {
		Print("Set age " + value);
		db.age = ScanInt(value);
		db.Flush();
	}
	else if (key == "gender") {
		Print("Set gender " + value);
		db.gender = ScanInt(value);
		db.Flush();
	}
	else if (key == "profile_image") {
		Print("Set profile image");
		
		if (value.GetCount() > Config::max_image_size) throw Exc("Invalid image received");
		db.profile_img = value;
		db.profile_img_hash = ImageHash(db.profile_img);
		db.Flush();
		
		StoreImageCache(db.profile_img_hash, db.profile_img);
		
		server->SendToAll(user_id, "profile " + IntStr(user_id) + " " + value);
	}
	out.Put32(ret);
}

void ActiveSession::Who(int user_id, Stream& out) {
	if (user_id < 0 || user_id >= server->db.GetUserCount()) {
		out.Put32(-1);
		out.Put32(0);
		return;
	}
	const UserDatabase& db = GetDatabase(user_id);
	out.Put32(user_id);
	out.Put32(db.name.GetCount());
	if (!db.name.IsEmpty())
		out.Put(db.name.Begin(), db.name.GetCount());
	out.Put32(db.age);
	out.Put32(db.gender);
	
	out.Put32(db.profile_img_hash);
	out.Put(&db.longitude, sizeof(double));
	out.Put(&db.latitude, sizeof(double));
	out.Put(&db.elevation, sizeof(double));
	
	out.Put32(db.channels.GetCount());
	for(int j = 0; j < db.channels.GetCount(); j++) {
		int k = db.channels[j];
		if (k < 0 || k >= server->db.channels.GetCount())
			out.Put32(0);
		else {
			const Channel& ch = server->db.channels[k];
			out.Put32(ch.name.GetCount());
			out.Put(ch.name.Begin(), ch.name.GetCount());
		}
	}
}

void ActiveSession::Get(Stream& in, Stream& out) {
	int user_id = LoginId(in);
	
	int r;
	int key_len;
	String key;
	r = in.Get(&key_len, sizeof(key_len));
	if (r != sizeof(key_len) || key_len < 0 || key_len > 200) throw Exc("Invalid key argument");
	key = in.Get(key_len);
	if (key.GetCount() != key_len) throw Exc("Invalid key received");
	
	const UserDatabase& db = GetDatabase(user_id);
	
	int64 size_pos = out.GetPos();
	out.SeekCur(sizeof(int));
	
	int i = key.Find(" ");
	Vector<String> args;
	if (i != -1) {
		args = Split(key.Mid(i+1), " ");
		key = key.Left(i);
	}
	
	if (key == "channellist") {
		server->lock.EnterRead();
		
		out.Put32(db.channels.GetCount());
		for(int i = 0; i < db.channels.GetCount(); i++) {
			int j = db.channels[i];
			if (j < 0 || j >= server->db.channels.GetCount())
				out.Put32(0);
			else {
				const Channel& ch = server->db.channels[j];
				out.Put32(ch.name.GetCount());
				out.Put(ch.name.Begin(), ch.name.GetCount());
			}
		}
		
		server->lock.LeaveRead();
	}
	else if (key == "allchannellist") {
		server->lock.EnterRead();
		VectorMap<String, int> ch_list;
		for(int i = 0; i < server->db.channels.GetCount(); i++) {
			const Channel& ch = server->db.channels[i];
			if (ch.name.Left(1) == ".") continue; // skip hidden channels
			int count = ch.users.GetCount();
			if (!count) continue;
			ch_list.Add(ch.name, count);
		}
		server->lock.LeaveRead();
		
		out.Put32(ch_list.GetCount());
		for(int i = 0; i < ch_list.GetCount(); i++) {
			const String& name = ch_list.GetKey(i);
			out.Put32(name.GetCount());
			out.Put(name.Begin(), name.GetCount());
			out.Put32(ch_list[i]);
		}
	}
	else if (key == "userlist") {
		server->lock.EnterRead();
		
		Index<int> userlist;
		server->GetUserlist(userlist, user_id);
		userlist.RemoveKey(user_id);
		out.Put32(userlist.GetCount());
		for(int i = 0; i < userlist.GetCount(); i++)
			Who(userlist[i], out);
		
		server->lock.LeaveRead();
	}
	else if (key == "profile_image_hash") {
		UserDatabase& db = GetDatabase(user_id);
		out.Put32(db.profile_img_hash);
	}
	else if (key == "image" && args.GetCount() == 1) {
		String img_folder = ConfigFile("images");
		RealizeDirectory(img_folder);
		String img_file = AppendFileName(img_folder, args[0] + ".bin");
		if (FileExists(img_file)) {
			String image_str = LoadFile(img_file);
			out.Put(image_str.Begin(), image_str.GetCount());
		}
		else {
			out.Put32(0);
		}
	}
	else if (key == "who" && args.GetCount() == 1) {
		int user_id = StrInt(args[0]);
		server->lock.EnterRead();
		Who(user_id, out);
		server->lock.LeaveRead();
	}
	
	out.Seek(size_pos);
	out.Put32(out.GetSize() - size_pos - 4);
	out.SeekEnd();
	out.Put32(0);
}

void ActiveSession::Join(Stream& in, Stream& out) {
	int user_id = LoginId(in);
	
	UserDatabase& db = GetDatabase(user_id);
	int ch_len;
	in.Get(&ch_len, sizeof(ch_len));
	if (ch_len < 0 || ch_len > 200) throw Exc("Invalid channel name");
	String ch_name = in.Get(ch_len);
	if (ch_name.GetCount() != ch_len) throw Exc("Invalid channel name");
	
	// Check if user is already joined at the channel
	server->lock.EnterRead();
	int ch_id = server->db.channels.Find(ch_name);
	server->lock.LeaveRead();
	
	if (ch_id == -1) {
		server->lock.EnterWrite();
		ch_id = server->db.channels.Find(ch_name);
		if (ch_id == -1) {
			ch_id = server->db.channels.GetCount();
			server->db.channels.Add(ch_name).name = ch_name;
			server->db.Flush();
		}
		server->lock.LeaveWrite();
	}
	int i = db.channels.Find(ch_id);
	if (i >= 0 || ch_id < 0) {
		out.Put32(1);
	} else {
		db.channels.Add(ch_id);
		db.Flush();
		
		server->JoinChannel(ch_name, user_id);
		
		out.Put32(0);
	}
}

void ActiveSession::Leave(Stream& in, Stream& out) {
	int user_id = LoginId(in);
	
	UserDatabase& db = GetDatabase(user_id);
	int ch_len;
	in.Get(&ch_len, sizeof(ch_len));
	if (ch_len < 0 || ch_len > 200) throw Exc("Invalid channel name");
	String ch_name = in.Get(ch_len);
	if (ch_name.GetCount() != ch_len) throw Exc("Invalid channel name");
	
	int ch_id = server->db.channels.Find(ch_name);
	int i = db.channels.Find(ch_id);
	if (i >= 0) {
		db.channels.Remove(i);
		db.Flush();
	}
	server->LeaveChannel(ch_name, user_id);
	
	out.Put32(0);
}

void ActiveSession::Location(Stream& in, Stream& out) {
	int user_id = LoginId(in);
	
	UserDatabase& db = GetDatabase(user_id);
	int r;
	double lat = 0, lon = 0, elev = 0;
	r = in.Get(&lat, sizeof(lat));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	r = in.Get(&lon, sizeof(lon));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	r = in.Get(&elev, sizeof(elev));
	if (r != sizeof(double)) throw Exc("Invalid location argument");
	
	db.SetLocation(lon, lat, elev);
	
	server->lock.EnterRead();
	Index<int> userlist;
	server->GetUserlist(userlist, user_id);
	userlist.RemoveKey(user_id);
	server->SendMessage(user_id, "loc " + IntStr(user_id) + " " + DblStr(lon) + " " + DblStr(lat) + " " + DblStr(elev), userlist);
	server->lock.LeaveRead();
	
	out.Put32(0);
}

void ActiveSession::Message(Stream& in, Stream& out) {
	int user_id = LoginId(in);
	
	int r, i;
	int recv_user_id, recv_msg_len;
	String recv_msg;
	r = in.Get(&recv_user_id, sizeof(recv_user_id));
	if (r != sizeof(recv_user_id)) throw Exc("Invalid message argument");
	r = in.Get(&recv_msg_len, sizeof(recv_msg_len));
	if (r != sizeof(recv_msg_len) || recv_msg_len < 0 || recv_msg_len > 10000000) throw Exc("Invalid message argument");
	recv_msg = in.Get(recv_msg_len);
	if (recv_msg.GetCount() != recv_msg_len) throw Exc("Invalid message received");
	
	server->lock.EnterRead();
	if (recv_user_id < 0 || recv_user_id >= server->db.GetUserCount())  {server->lock.LeaveRead(); out.Put32(1); return;}
	server->lock.LeaveRead();
	
	const MessageRef& ref = server->IncReference("msg " + recv_msg, 1);
	UserDatabase& db = GetDatabase(recv_user_id);
	
	db.lock.Enter();
	InboxMessage& msg = db.inbox.Add();
	msg.msg = ref.hash;
	msg.sender_id = user_id;
	db.lock.Leave();
	
	out.Put32(0);
}

void ActiveSession::Poll(Stream& in, Stream& out) {
	int user_id = LoginId(in);
	
	UserDatabase& db = GetDatabase(user_id);
	
	Vector<InboxMessage> tmp;
	db.lock.Enter();
	Swap(db.inbox, tmp);
	db.lock.Leave();
	
	// double check that sender_id != user_id
	for(int i = 0; i < db.inbox.GetCount(); i++) {
		if (db.inbox[i].sender_id == user_id) {
			db.inbox.Remove(i);
			i--;
		}
	}
	
	int count = tmp.GetCount();
	out.Put32(count);
	for(int i = 0; i < count; i++) {
		const InboxMessage& im = tmp[i];
		out.Put32(im.sender_id);
		
		MessageRef& ref = server->GetReference(im.msg);
		int msg_len = ref.msg.GetCount();
		out.Put32(msg_len);
		if (msg_len > 0)
			out.Put(ref.msg.Begin(), msg_len);
		
		server->DecReference(ref);
	}
}

void ActiveSession::ChannelMessage(Stream& in, Stream& out) {
	int user_id = LoginId(in);
	
	int r, i;
	int recv_ch_len, recv_msg_len;
	String recv_ch, recv_msg;
	r = in.Get(&recv_ch_len, sizeof(recv_ch_len));
	if (r != sizeof(recv_ch_len) || recv_ch_len > 10000) throw Exc("Invalid message argument");
	recv_ch = in.Get(recv_ch_len);
	if (recv_ch.GetCount() != recv_ch_len) throw Exc("Invalid message argument");
	r = in.Get(&recv_msg_len, sizeof(recv_msg_len));
	if (r != sizeof(recv_msg_len) || recv_msg_len < 0 || recv_msg_len > 10000000) throw Exc("Invalid message argument");
	recv_msg = in.Get(recv_msg_len);
	if (recv_msg.GetCount() != recv_msg_len) throw Exc("Invalid message received");
	
	server->lock.EnterRead();
	i = server->db.channels.Find(recv_ch);
	if (i < 0)  {server->lock.LeaveRead(); out.Put32(1); return;}
	Channel& ch = server->db.channels[i];
	
	const MessageRef& ref = server->IncReference("chmsg " + recv_ch + " " + recv_msg, ch.users.GetCount() - 1);
	
	for(int i = 0; i < ch.users.GetCount(); i++) {
		int id = ch.users[i];
		if (id == user_id) continue;
		UserDatabase& db = GetDatabase(id);
		db.lock.Enter();
		InboxMessage& msg = db.inbox.Add();
		msg.msg = ref.hash;
		msg.sender_id = user_id;
		db.lock.Leave();
	}
	server->lock.LeaveRead();
	
	out.Put32(0);
}

void ActiveSession::StoreImageCache(int hash, const String& image_str) {
	String img_folder = ConfigFile("images");
	RealizeDirectory(img_folder);
	String img_file = AppendFileName(img_folder, IntStr(hash) + ".bin");
	FileOut fout(img_file);
	fout << image_str;
	fout.Close();
}
