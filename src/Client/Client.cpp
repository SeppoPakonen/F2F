#include "Client.h"
#include "AES.h"
#include <plugin/jpg/jpg.h>

namespace Config {

INI_STRING(master_addr, "93.170.105.68", "Master server's address");
INI_INT(master_port, 17123, "Master server's port");

};

void Print(const String& s) {
	static Mutex lock;
	lock.Enter();
	Cout() << s;
	Cout().PutEol();
	LOG(s);
	lock.Leave();
}



Client::Client() {
	Icon(Images::icon());
	Title("F2F Client program");
	Sizeable().MaximizeBox().MinimizeBox();
	
	Add(split.SizePos());
	split << irc << map;
	split.Horz();
	split.SetPos(6666);
	
	irc.WhenCommand << THISBACK(Command);
	irc.WhenChannelChanged << THISBACK(RefreshGuiChannel);
	
	map.Set(Pointf(25.46748, 65.05919));
	map.WhenHome << THISBACK(ChangeLocation);
}

Client::~Client() {
	running = false;
	if (!s.IsEmpty()) s->Close();
	while (!stopped) Sleep(100);
}

bool Client::Connect() {
	if (s.IsEmpty() || !s->IsOpen()) {
		is_logged_in = false;
		
		if (!s.IsEmpty()) s.Clear();
		s.Create();
		
		if(!s->Connect(addr, port)) {
			Print("Client " + IntStr(user_id) + " Unable to connect to server!");
			return false;
		}
		
		// Pre-command: skip pre-commands
		int i = -1;
		int r = s->Put(&i, sizeof(int));
		if (r != sizeof(int))
			return false;
	}
	return true;
}

bool Client::RegisterScript() {
	if (!is_registered) {
		try {
			Register();
			is_registered = true;
			StoreThis();
		}
		catch (Exc e) {
			return false;
		}
	}
	return true;
}

bool Client::LoginScript() {
	if (!is_logged_in) {
		try {
			Login();
			RefreshChannellist();
			RefreshUserlist();
			is_logged_in = true;
			PostCallback(THISBACK(RefreshGui));
		}
		catch (Exc e) {
			return false;
		}
	}
	return true;
}

void Client::SetName(String s) {
	if (user_name == s) return;
	try {
		if (Set("name", s))
			user_name = s;
	}
	catch (Exc e) {
		Print("Changing name failed");
	}
}

void Client::SetImage(Image i) {
	unsigned hash = 0;
	try {
		String hash_str;
		Get("profile_image_hash", hash_str);
		hash = ScanInt64(hash_str);
	}
	catch (Exc e) {
		Print("Getting existing image hash failed");
	}
	while (true) {
		JPGEncoder jpg;
		jpg.Quality(80);
		String imgstr = jpg.SaveString(i);
		
		if (imgstr.GetCount() > 100000) {
			i = RescaleFilter(i, i.GetSize() * 0.5, FILTER_BILINEAR);
		}
		else {
			if (hash != imgstr.GetHashValue()) {
				try {
					Set("profile_image", imgstr);
				}
				catch (Exc e) {
					Print("Changing profile image failed");
				}
			}
			break;
		}
	}
}

void Client::StoreImageCache(const String& image_str) {
	unsigned hash = image_str.GetHashValue();
	String img_folder = ConfigFile("images");
	RealizeDirectory(img_folder);
	String img_file = AppendFileName(img_folder, IntStr64(hash) + ".bin");
	FileOut fout(img_file);
	fout.Put(image_str.Begin(), image_str.GetCount());
	fout.Close();
}

bool Client::HasCachedImage(unsigned hash) {
	String img_folder = ConfigFile("images");
	String img_file = AppendFileName(img_folder, IntStr64(hash) + ".bin");
	return FileExists(img_file);
}

String Client::LoadImageCache(unsigned hash) {
	String img_folder = ConfigFile("images");
	String img_file = AppendFileName(img_folder, IntStr64(hash) + ".bin");
	return LoadFile(img_file);
}

void Client::HandleConnection() {
	Print("Client " + IntStr(user_id) + " Running");
	
	int count = 0;
	
	while (!Thread::IsShutdownThreads() && running) {
		
		Connect();
		
		try {
			while (!Thread::IsShutdownThreads() && s->IsOpen() && running) {
				RegisterScript();
				LoginScript();
				
				Poll();
				Sleep(1000);
				count++;
			}
		}
		catch (Exc e) {
			Print("Client " + IntStr(user_id) + " Error: " + e);
			count = min(count, 1);
		}
		catch (const char* e) {
			Print("Client " + IntStr(user_id) + " Error: " + e);
			count = min(count, 1);
		}
		catch (...) {
			Print("Client " + IntStr(user_id) + " Unexpected error");
			break;
		}
		
		s.Clear();
	}
	
	Print("Client " + IntStr(user_id) + " Stopping");
	stopped = true;
}

void Client::Call(Stream& out, Stream& in) {
	int r;
	
	AESEncoderStream enc(10000000, "passw0rdpassw0rd");
	out.Seek(0);
	String out_str = out.Get(out.GetSize());
	if (out_str.GetCount() % AES_BLOCK_SIZE != 0)
		out_str.Cat(0, AES_BLOCK_SIZE - (out_str.GetCount() % AES_BLOCK_SIZE));
	enc.AddData(out_str);
	String out_data = enc.GetEncryptedData();
	int out_size = out_data.GetCount();
	
	call_lock.Enter();
	r = s->Put(&out_size, sizeof(out_size));
	if (r != sizeof(out_size)) {call_lock.Leave(); throw Exc("Data sending failed");}
	r = s->Put(out_data.Begin(), out_data.GetCount());
	if (r != out_data.GetCount()) {call_lock.Leave(); throw Exc("Data sending failed");}
	
	s->Timeout(30000);
	int in_size;
	r = s->Get(&in_size, sizeof(in_size));
	if (r != sizeof(in_size) || in_size < 0 || in_size >= 10000000) {call_lock.Leave(); throw Exc("Received invalid size");}
	
	String in_data = s->Get(in_size);
	if (in_data.GetCount() != in_size) {call_lock.Leave(); throw Exc("Received invalid data");}
	call_lock.Leave();
	
	AESDecoderStream dec("passw0rdpassw0rd");
	dec.AddData(in_data);
	int64 pos = in.GetPos();
	in << dec.GetDecryptedData();
	in.Seek(pos);
}

void Client::Register() {
	StringStream out, in;
	
	out.Put32(100);
	
	Call(out, in);
	
	in.Get(&user_id, sizeof(int));
	pass = in.Get(8);
	if (pass.GetCount() != 8) throw Exc("Invalid password");
	
	Print("Client " + IntStr(user_id) + " registered (pass " + pass + ")");
}

void Client::Login() {
	StringStream out, in;
	
	out.Put32(200);
	out.Put32(user_id);
	out.Put(pass.Begin(), pass.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Login failed");
	
	int name_len = in.Get32();
	user_name = in.Get(name_len);
	
	Print("Client " + IntStr(user_id) + " logged in (" + IntStr(user_id) + ", " + pass + ") nick: " + user_name);
}

bool Client::Set(const String& key, const String& value) {
	StringStream out, in;
	
	out.Put32(300);
	
	out.Put32(key.GetCount());
	out.Put(key.Begin(), key.GetCount());
	out.Put32(value.GetCount());
	out.Put(value.Begin(), value.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret == 1) {
		Print("Client " + IntStr(user_id) + " set " + key + " FAILED");
		return false;
	}
	else if (ret != 0) throw Exc("Setting value failed");
	
	Print("Client " + IntStr(user_id) + " set " + key);
	return true;
}

void Client::Get(const String& key, String& value) {
	StringStream out, in;
	
	out.Put32(400);
	
	out.Put32(key.GetCount());
	out.Put(key.Begin(), key.GetCount());
	
	Call(out, in);
	
	int value_len = in.Get32();
	value = in.Get(value_len);
	if (value.GetCount() != value_len) throw Exc("Getting value failed");
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Getting value failed");
	
	Print("Client " + IntStr(user_id) + " get " + key);
}

void Client::Join(String channel) {
	if (channel.IsEmpty()) return;
	StringStream out, in;
	
	out.Put32(500);
	
	int ch_len = channel.GetCount();
	out.Put32(ch_len);
	out.Put(channel.Begin(), channel.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret == 1) {
		Print("Client " + IntStr(user_id) + " WAS JOINED ALREADY AT channel " + channel);
		return;
	}
	
	if (ret != 0) throw Exc("Joining channel failed");
	
	my_channels.FindAdd(channel);
	channels.FindAdd(channel);
	PostCallback(THISBACK(RefreshGui));
	
	Print("Client " + IntStr(user_id) + " joined channel " + channel);
}

void Client::Leave(String channel) {
	if (channel.IsEmpty()) return;
	StringStream out, in;
	
	out.Put32(600);
	
	int ch_len = channel.GetCount();
	out.Put32(ch_len);
	out.Put(channel.Begin(), channel.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Leaving channel failed");
	
	my_channels.RemoveKey(channel);
	PostCallback(THISBACK(RefreshGui));
	
	Print("Client " + IntStr(user_id) + " left from channel " + channel);
}

void Client::Message(int recv_user_id, const String& msg) {
	if (recv_user_id < 0) return;
	StringStream out, in;
	
	out.Put32(700);
	
	out.Put32(recv_user_id);
	out.Put32(msg.GetCount());
	out.Put(msg.Begin(), msg.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Message sending failed");
	
	Print("Client " + IntStr(user_id) + " sent message from " + IntStr(user_id) + " to " + IntStr(recv_user_id) + ": " + msg);
}

void Client::Poll() {
	StringStream out, in;
	
	out.Put32(800);
	
	Call(out, in);
	
	lock.Enter();
	
	VectorMap<String, int> key_time;
	Vector<Tuple<int, String> > join_list;
	
	int count = in.Get32();
	if (count < 0 || count >= 10000) {lock.Leave(); throw Exc("Polling failed");}
	for(int i = 0; i < count; i++) {
		TimeStop ts;
		
		int sender_id = in.Get32();
		int msg_len = in.Get32();
		String message;
		if (msg_len > 0)
			message = in.Get(msg_len);
		if (message.GetCount() != msg_len) {lock.Leave(); throw Exc("Polling failed");}
		Print("Client " + IntStr(user_id) + " received from " + IntStr(sender_id) + ": " + IntStr(message.GetCount()));
		
		int j = message.Find(" ");
		if (j == -1) continue;
		String key = message.Left(j);
		message = message.Mid(j + 1);
		
		if (key == "msg") {
			String ch_name = "user" + IntStr(sender_id);
			ASSERT(sender_id != this->user_id);
			User& u = users.GetAdd(sender_id);
			my_channels.FindAdd(ch_name);
			Channel& ch = channels.GetAdd(ch_name);
			ch.userlist.FindAdd(sender_id);
			ch.Post(sender_id, u.name, message);
			PostCallback(THISBACK(RefreshGui));
		}
		else if (key == "chmsg") {
			ASSERT(sender_id != this->user_id);
			User& u = users.GetAdd(sender_id);
			j = message.Find(" ");
			String ch_name = message.Left(j);
			message = message.Mid(j + 1);
			Channel& ch = channels.GetAdd(ch_name);
			ch.Post(sender_id, u.name, message);
			PostCallback(THISBACK(RefreshGui));
		}
		else if (key == "join") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 2) {lock.Leave(); throw Exc("Polling argument error");}
			int user_id = StrInt(args[0]);
			ASSERT(user_id != this->user_id);
			String ch_name = args[1];
			User& u = users.GetAdd(user_id);
			u.user_id = user_id;
			u.channels.FindAdd(ch_name);
			Channel& ch = channels.GetAdd(ch_name);
			ch.userlist.FindAdd(user_id);
			join_list.Add(Tuple<int,String>(user_id, ch_name));
		}
		else if (key == "leave") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 2) {lock.Leave(); throw Exc("Polling argument error");}
			int user_id = StrInt(args[0]);
			ASSERT(user_id != this->user_id);
			String ch_name = args[1];
			User& u = users.GetAdd(user_id);
			u.user_id = user_id;
			u.channels.RemoveKey(ch_name);
			Channel& ch = channels.GetAdd(ch_name);
			ch.userlist.RemoveKey(user_id);
			ch.Post(-1, "Server", "User " + u.name + " left channel " + ch_name);
			if (u.channels.IsEmpty())
				users.RemoveKey(user_id);
			PostCallback(THISBACK(RefreshGui));
		}
		else if (key == "name") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 2) {lock.Leave(); throw Exc("Polling argument error");}
			int user_id = StrInt(args[0]);
			ASSERT(user_id != this->user_id);
			String user_name = args[1];
			User& u = users.GetAdd(user_id);
			for(int i = 0; i < u.channels.GetCount(); i++) {
				Channel& ch = channels.GetAdd(u.channels[i]);
				ch.Post(-1, "Server", Format("User %s changed name to %s", u.name, user_name));
			}
			u.name = user_name;
			u.user_id = user_id;
			PostCallback(THISBACK(RefreshGui));
		}
		else if (key == "loc") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 4) {lock.Leave(); throw Exc("Polling argument error");}
			int user_id = StrInt(args[0]);
			double lon = StrDbl(args[1]);
			double lat = StrDbl(args[2]);
			double elev = StrDbl(args[3]);
			int i = users.Find(user_id);;
			if (i == -1) continue;
			User& u = users[i];
			u.last_update = GetSysTime();
			u.longitude = lon;
			u.latitude = lat;
			u.elevation = elev;
			for(int i = 0; i < u.channels.GetCount(); i++) {
				Channel& ch = channels.GetAdd(u.channels[i]);
				ch.Post(-1, "Server", Format("User %s changed location to %f %f %f", u.name, lon, lat, elev));
			}
		}
		else if (key == "profile") {
			j = message.Find(" ");
			if (j == -1) {lock.Leave(); throw Exc("System error");}
			String user_id_str = message.Left(j);
			int user_id = ScanInt(user_id_str);
			message = message.Mid(j+1);
			j = users.Find(user_id);
			StoreImageCache(message);
			if (j >= 0) {
				User& u = users[j];
				u.profile_image = StreamRaster::LoadStringAny(message);
				u.profile_image_hash = message.GetHashValue();
				for(int i = 0; i < u.channels.GetCount(); i++) {
					Channel& ch = channels.GetAdd(u.channels[i]);
					ch.Post(-1, "Server", Format("User %s updated profile image", u.name));
				}
			}
		}
		
		key_time.GetAdd(key, 0) += ts.Elapsed();
	}
	
	if (!join_list.IsEmpty()) {
		
		for(int i = 0; i < join_list.GetCount(); i++) {
			int user_id = join_list[i].a;
			String ch_name = join_list[i].b;
			
			String who;
			Get("who " + IntStr(user_id), who);
			MemReadStream in(who.Begin(), who.GetCount());
			
			Who(in);
			
			User& u = users.GetAdd(user_id);
			RefreshUserImage(u);
			
			Channel& ch = channels.GetAdd(ch_name);
			ch.Post(-1, "Server", Format("User %s joined channel %s", u.name, ch_name));
		}
		
		PostCallback(THISBACK(RefreshGui));
	}
	
	DUMPM(key_time);
	
	lock.Leave();
}

void Client::ChangeLocation(Pointf coord) {
	Location l;
	l.longitude = coord.x;
	l.latitude = coord.y;
	l.elevation = 0.0;
	SendLocation(l);
	
	for(int i = 0; i < my_channels.GetCount(); i++) {
		Channel& ch = channels.GetAdd(my_channels[i]);
		ch.Post(-1, "Server", Format("I changed location to %f %f %f", l.longitude, l.latitude, l.elevation));
	}
}

void Client::SendLocation(const Location& l) {
	StringStream out, in;
	
	out.Put32(900);
	
	out.Put(&l.latitude, sizeof(l.latitude));
	out.Put(&l.longitude, sizeof(l.longitude));
	out.Put(&l.elevation, sizeof(l.elevation));
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Updating location failed");
	
	Print("Client " + IntStr(user_id) + " updated location");
}

void Client::SendChannelMessage(String channel, const String& msg) {
	if (channel.IsEmpty()) return;
	StringStream out, in;
	
	out.Put32(1000);
	
	out.Put32(channel.GetCount());
	out.Put(channel.Begin(), channel.GetCount());
	out.Put32(msg.GetCount());
	out.Put(msg.Begin(), msg.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Message sending failed");
	
	Print("Client " + IntStr(user_id) + " sent message from " + IntStr(user_id) + " to " + channel + ": " + msg);
}

void Client::RefreshChannellist() {
	String channellist_str;
	Get("channellist", channellist_str);
	MemReadStream in(channellist_str.Begin(), channellist_str.GetCount());
	
	Index<String> ch_rem;
	for(int i = 0; i < my_channels.GetCount(); i++) ch_rem.Add(my_channels[i]);
	
	int ch_count = in.Get32();
	bool fail = false;
	for(int i = 0; i < ch_count; i++) {
		int name_len = in.Get32();
		if (name_len <= 0) continue;
		String name = in.Get(name_len);
		
		if (ch_rem.Find(name) != -1) ch_rem.RemoveKey(name);
		
		my_channels.FindAdd(name);
		Channel& ch = channels.GetAdd(name);
	}
	if (fail) throw Exc("Getting channellist failed");
	
	for(int i = 0; i < ch_rem.GetCount(); i++)
		my_channels.RemoveKey(ch_rem[i]);
	
	if (!my_channels.IsEmpty() && irc.GetActiveChannel().IsEmpty()) {
		irc.SetActiveChannel(my_channels[0]);
		PostCallback(THISBACK(RefreshGuiChannel));
	}
	
	Print("Client " + IntStr(user_id) + " updated channellist (size " + IntStr(ch_count) + ")");
}

bool Client::Who(Stream& in) {
	bool success = true;
	int user_id = in.Get32();
	int name_len = in.Get32();
	if (name_len <= 0 || user_id < 0) return false;
	String name = in.Get(name_len);
	
	User& u = users.GetAdd(user_id);
	u.user_id = user_id;
	u.name = name;
	if (u.name.GetCount() != name_len) success = false;
	
	u.profile_image_hash = in.Get32();
	in.Get(&u.longitude, sizeof(double));
	in.Get(&u.latitude, sizeof(double));
	in.Get(&u.elevation, sizeof(double));
	
	int channel_count = in.Get32();
	if (channel_count < 0 || channel_count >= 200) return false;
	for(int j = 0; j < channel_count; j++) {
		int ch_len = in.Get32();
		String ch_name = in.Get(ch_len);
		u.channels.FindAdd(ch_name);
		Channel& ch = channels.GetAdd(ch_name);
		ch.userlist.FindAdd(user_id);
	}
	
	return success;
}

void Client::RefreshUserlist() {
	String userlist_str;
	Get("userlist", userlist_str);
	MemReadStream in(userlist_str.Begin(), userlist_str.GetCount());
	
	int user_count = in.Get32();
	bool fail = false;
	for(int i = 0; i < user_count; i++)
		fail |= !Who(in);
	if (fail) throw Exc("Getting userlist failed");
	
	for(int i = 0; i < users.GetCount(); i++) {
		RefreshUserImage(users[i]);
	}
	
	Print("Client " + IntStr(user_id) + " updated userlist (size " + IntStr(user_count) + ")");
}

void Client::RefreshUserImage(User& u) {
	String image_str;
	
	if (!HasCachedImage(u.profile_image_hash)) {
		// Fetch image
		Get("image " + IntStr64(u.profile_image_hash), image_str);
		
		// Store to hard drive
		StoreImageCache(image_str);
		
	} else {
		image_str = LoadImageCache(u.profile_image_hash);
	}
	
	// Load to memory
	u.profile_image = StreamRaster::LoadStringAny(image_str);
}

void Client::RefreshGui() {
	lock.Enter();
	
	ChannelList& cl = irc.GetChannelList();
	cl.SetChannelCount(my_channels.GetCount());
	
	for(int i = 0; i < my_channels.GetCount(); i++) {
		String ch_name = my_channels[i];
		Channel& ch = channels.GetAdd(ch_name);
		cl.SetChannelName(i, ch_name);
		cl.SetChannelUnread(i, ch.unread);
	}
	
	String active_channel = irc.GetActiveChannel();
	if (!active_channel.IsEmpty()) {
		Channel& ch = channels.GetAdd(active_channel);
		
		for(int i = ch.messages.GetCount() - ch.unread; i < ch.messages.GetCount(); i++) {
			const ChannelMessage& msg = ch.messages[i];
			irc.Post(msg);
		}
		ch.unread = 0;
		
		int count = ch.userlist.GetCount();
		irc.SetUserCount(count);
		map.SetPersonCount(count);
		for(int i = 0; i < min(count, ch.userlist.GetCount()); i++) {
			User& u = users.GetAdd(ch.userlist[i]);
			irc.SetUser(i, u.profile_image, u.name);
			map.SetPerson(i, Pointf(u.longitude, u.latitude), u.profile_image);
		}
	}
	
	lock.Leave();
	
	map.map.Refresh();
}

void Client::RefreshGuiChannel() {
	String active_channel = irc.GetActiveChannel();
	if (active_channel.IsEmpty()) return;
	
	Channel& ch = channels.GetAdd(active_channel);
		
	for(int i = 0; i < ch.messages.GetCount(); i++) {
		const ChannelMessage& msg = ch.messages[i];
		irc.Post(msg);
	}
	ch.unread = 0;
	
	irc.SetUserCount(ch.userlist.GetCount());
	for(int i = 0; i < ch.userlist.GetCount(); i++) {
		User& u = users.GetAdd(ch.userlist[i]);
		irc.SetUser(i, u.profile_image, u.name);
	}
	
	RefreshGui();
}

void Client::Command(String cmd) {
	if (cmd.IsEmpty()) return;
	
	if (cmd[0] == '/') {
		Vector<String> args = Split(cmd.Mid(1), " ");
		if (args.IsEmpty()) return;
		String key = args[0];
		if (key == "join") {
			if (args.GetCount() < 2) return;
			String ch_name = args[1];
			Join(ch_name);
			irc.SetActiveChannel(ch_name);
			try {
				RefreshUserlist();
			}
			catch (Exc e) {
			
			}
			RefreshGuiChannel();
		}
		else if (key == "leave") {
			if (args.GetCount() < 2) return;
			String channel = args[1];
			Leave(channel);
			if (irc.GetActiveChannel() == channel) {
				if (channels.IsEmpty())
					irc.SetActiveChannel("");
				else
					irc.SetActiveChannel(channels.GetKey(0));
			}
			RefreshGui();
		}
		else if (key == "name") {
			if (args.GetCount() < 2) return;
			String name = args[1];
			if (Set("name", name)) {
				user_name = name;
				RefreshGui();
			}
		}
		else if (key == "msg") {
			if (args.GetCount() < 2) return;
			String user = args[1];
			args.Remove(0, 2);
			String message = Upp::Join(args, " ");
			int recv_id = -1;
			for(int i = 0; i < users.GetCount(); i++) {
				if (users[i].name == user) {
					recv_id = users.GetKey(i);
					break;
				}
			}
			if (recv_id == -1) return;
			String ch_name = "user" + IntStr(recv_id);
			if (recv_id == this->user_id) return;
			User& u = users.GetAdd(recv_id);
			my_channels.FindAdd(ch_name);
			Channel& ch = channels.GetAdd(ch_name);
			ch.userlist.FindAdd(recv_id);
			ch.Post(user_id, user_name, message);
			try {
				Message(recv_id, message);
			}
			catch (Exc e) {
				ch.Post(-1, "F2F client", "Problems sending messages...");
			}
			PostCallback(THISBACK(RefreshGui));
		}
	}
	else {
		String active_channel = irc.GetActiveChannel();
		Channel& ch = channels.GetAdd(active_channel);
		ch.Post(user_id, user_name, cmd);
		if (active_channel.Left(4) != "user") {
			RefreshGui();
			try {
				SendChannelMessage(active_channel, cmd);
			}
			catch (Exc e) {
				ch.Post(-1, "F2F client", "Problems sending messages...");
			}
		} else {
			int recv_id = ScanInt(active_channel.Mid(4));
			int i = users.Find(recv_id);
			if (i == -1) return;
			User& u = users.GetAdd(recv_id);
			try {
				Message(recv_id, cmd);
			}
			catch (Exc e) {
				ch.Post(-1, "F2F client", "Problems sending messages...");
			}
			PostCallback(THISBACK(RefreshGui));
		}
	}
}











void Channel::Post(int user_id, String user_name, const String& msg) {
	ChannelMessage& m = messages.Add();
	m.received = GetSysTime();
	m.message = msg;
	m.sender_id = user_id;
	m.sender_name = user_name;
	unread++;
}










ChannelList::ChannelList() {
	unread_style = Button::StyleNormal();
	unread_style.textcolor[0] = Red();
}

void ChannelList::Layout() {
	Size sz(GetSize());
	int y = 0;
	for(int i = 0; i < buttons.GetCount(); i++) {
		Button& b = buttons[i];
		b.SetRect(0, y, sz.cx, 30);
		y += 30;
	}
}

void ChannelList::SetChannelCount(int count) {
	for(int i = buttons.GetCount()-1; i > count; i--) {
		RemoveChild(&buttons[i]);
		buttons.Remove(i);
	}
	for(int i = buttons.GetCount(); i < count; i++) {
		Button& b = buttons.Add();
		Add(b);
		b << THISBACK1(SetChannel, i);
	}
	Layout();
}

void ChannelList::SetChannelName(int i, String s) {
	buttons[i].SetLabel(s);
}

void ChannelList::SetChannelUnread(int i, int unread) {
	Button& b = buttons[i];
	
	if (unread)
		b.SetStyle(unread_style);
	else
		b.SetStyle(Button::StyleNormal());
}











IrcCtrl::IrcCtrl() {
	int width = 200;
	Add(channels.LeftPos(0, width).VSizePos());
	Add(chat.HSizePos(width, width).VSizePos(0, 30));
	Add(users.RightPos(0, width).VSizePos());
	Add(cmd.BottomPos(0, 30).HSizePos(width, width));
	
	cmd.WhenEnter << THISBACK(Command);
	
	chat.AddColumn("Time");
	chat.AddColumn("From");
	chat.AddColumn("Message");
	chat.ColumnWidths("2 1 8");
	
	users.AddColumn("Image");
	users.AddColumn("Name");
	users.ColumnWidths("1 3");
	users.SetLineCy(32);
	
	channels.WhenChannel << THISBACK(SetChannel);
}

void IrcCtrl::Command() {
	String cmdstr = cmd.GetData();
	if (cmdstr.IsEmpty()) return;
	cmd.SetData("");
	WhenCommand(cmdstr);
}

void IrcCtrl::Post(const ChannelMessage& msg) {
	chat.Add(msg.received, msg.sender_name, msg.message);
	chat.ScrollEnd();
}

void IrcCtrl::SetUserCount(int i) {
	users.SetCount(i);
}

void IrcCtrl::SetUser(int i, Image img, String s) {
	users.Set(i, 0, img);
	users.SetDisplay(i, 0, ImageDisplay());
	users.Set(i, 1, s);
}














StartupDialog::StartupDialog(Client& c) : cl(c), sd(*this) {
	CtrlLayout(*this, "F2F startup");
	
	LoadThis();
	
	image.SetImage(profile_image);
	nick.SetData(name);
	addr.SetData(srv_addr);
	port.SetData(srv_port);
	auto_connect.Set(autoconnect);
	
	selectserver <<= THISBACK(SelectServer);
	selectimage <<= THISBACK(SelectImage);
	
	nick.SetFocus();
	Enable(true);
}

void StartupDialog::Enable(bool b) {
	nick.Enable(b);
	addr.Enable(b);
	port.Enable(b);
	auto_connect.Enable(b);
	selectimage.Enable(b);
	selectserver.Enable(b);
	connect.Enable(b);
	if (b) {
		connect.WhenAction = THISBACK(StartTryConnect);
		connect.SetLabel(t_("Connect"));
	} else {
		connect.WhenAction = THISBACK(StartStopConnect);
		connect.SetLabel(t_("Stop"));
	}
}

void StartupDialog::TryConnect() {
	name = nick.GetData();
	autoconnect = auto_connect.Get();
	srv_addr = addr.GetData();
	srv_port = port.GetData();
	StoreThis();
	
	if (profile_image.GetSize() == Size(0,0))
		PostCallback(THISBACK1(SetError, "Profile image is not set"));
	else if (name.IsEmpty())
		PostCallback(THISBACK1(SetError, "Profile name is empty"));
	else if (srv_addr.IsEmpty())
		PostCallback(THISBACK1(SetError, "Server address is empty"));
	else if (Connect())
		PostCallback(THISBACK(Close0));
	else
		PostCallback(THISBACK1(SetError, "Connecting server failed"));
	PostCallback(THISBACK1(Enable, true));
}

bool StartupDialog::Connect() {
	cl.SetAddress(srv_addr, srv_port);
	cl.LoadThis();
	cl.CloseConnection();
	return cl.Connect() && cl.RegisterScript() && cl.LoginScript();
}

void StartupDialog::StopConnect() {
	cl.CloseConnection();
}

void StartupDialog::SelectServer() {
	sd.RefreshAddresses();
	if (sd.Execute() == IDOK) {
		int i = sd.serverlist.GetCursor();
		if (i >= 0 && i < sd.servers.GetCount()) {
			i = sd.serverlist.Get(i, 6);
			addr.SetData(sd.servers[i].addr);
			port.SetData(sd.servers[i].port);
			StoreThis();
		}
	}
}

void StartupDialog::SelectImage() {
	PreviewImage img;
	FileSel fs;
	
	fs.Type("Image file(s)", "*.jpg *.gif *.png *.bmp");
	fs.Preview(img);
	
	if (fs.ExecuteOpen() == IDOK) {
		String path = ~fs;
		profile_image = StreamRaster::LoadFileAny(path);
		image.SetImage(profile_image);
		StoreThis();
	}
}

void StartupDialog::Setup() {
	cl.SetName(name);
	cl.SetImage(profile_image);
}











ServerDialog::ServerDialog(StartupDialog& sd) : sd(sd) {
	CtrlLayoutOKCancel(*this, "Select server");
	
	serverlist.AddColumn("Title");
	serverlist.AddColumn("Ping");
	serverlist.AddColumn("Users");
	serverlist.AddColumn("Max users");
	serverlist.AddColumn("Address");
	serverlist.AddColumn("Port");
	serverlist.AddIndex();
	serverlist.AddIndex();
	serverlist.ColumnWidths("6 2 2 2 3 2");
	
	Thread::Start(THISBACK(RefreshAddresses));
}

void ServerDialog::RefreshAddresses() {
	Print("Getting server list from the master server");
	
	servers.Clear();
	
	try {
		TcpSocket master;
		if (!master.Connect((String)Config::master_addr, Config::master_port))
			throw Exc("Unable to connect the master server list");
		uint16 port = 0;
		int r = master.Put(&port, sizeof(uint16));
		if (r != sizeof(uint16)) throw Exc("Master server connection failed");
		
		int server_count;
		r = master.Get(&server_count, sizeof(int));
		if (r != sizeof(int)) throw Exc("Master server connection failed");
		
		for(int i = 0; i < server_count; i++) {
			Server& s = servers.Add();
			
			int addr_len = 0;
			r = master.Get(&addr_len, sizeof(int));
			if (r != sizeof(int) || addr_len < 0 || addr_len > 200) throw Exc("Master server connection failed");
			
			s.addr = master.Get(addr_len);
			if (s.addr.GetCount() != addr_len) throw Exc("Master server connection failed");
			
			r = master.Get(&s.port, sizeof(uint16));
			if (r != sizeof(uint16)) throw Exc("Master server connection failed");
			
		}
	}
	catch (Exc e) {
		
		return;
	}
	
	for(auto& s : socks) {s.Timeout(1); s.Close();} while (running > 0) Sleep(100);
	
	running = servers.GetCount();
	socks.SetCount(running);
	for(int i = 0; i < servers.GetCount(); i++)
		Thread::Start(THISBACK1(TestConnection, i));
}

void ServerDialog::TestConnection(int i) {
	Server& server = servers[i];
	TcpSocket& sock = socks[i];
	
	try {
		sock.Timeout(10000);
		
		if (!sock.Connect(server.addr, server.port))
			throw Exc("Couldn't connect " + server.addr + ":" + IntStr(server.port));
		
		TimeStop ts;
		
		int r, datalen = 0;
		r = sock.Put(&datalen, sizeof(int));
		if (r != sizeof(int)) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		int greet_len;
		r = sock.Get(&greet_len, sizeof(int));
		if (r != sizeof(int) || greet_len < 0 || greet_len > 1000) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		server.greeting = sock.Get(greet_len);
		
		r = sock.Get(&server.sessions, sizeof(int));
		if (r != sizeof(int)) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		r = sock.Get(&server.max_sessions, sizeof(int));
		if (r != sizeof(int)) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		server.elapsed = ts.Elapsed();
	}
	catch (Exc e) {
		Print(e);
	}
	
	PostCallback(THISBACK(Data));
	running--;
}

void ServerDialog::Data() {
	for(int i = 0; i < servers.GetCount(); i++) {
		const Server& s = servers[i];
		serverlist.Set(i, 0, s.greeting);
		serverlist.Set(i, 1, (int)s.elapsed);
		serverlist.Set(i, 2, s.sessions);
		serverlist.Set(i, 3, s.max_sessions);
		serverlist.Set(i, 4, s.addr);
		serverlist.Set(i, 5, s.port);
		serverlist.Set(i, 6, i);
	}
	serverlist.SetSortColumn(2, true);
}


