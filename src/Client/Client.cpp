#include "Client.h"
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

double DegreesToRadians(double degrees) {
  return degrees * M_PI / 180.0;
}

double CoordinateDistanceKM(Pointf a, Pointf b) {
	double earth_radius_km = 6371.0;
	
	double dLat = DegreesToRadians(b.y - a.y);
	double dLon = DegreesToRadians(b.x - a.x);
	
	a.y = DegreesToRadians(a.y);
	b.y = DegreesToRadians(b.y);
	
	double d = sin(dLat/2) * sin(dLat/2) +
		sin(dLon/2) * sin(dLon/2) * cos(a.y) * cos(b.y);
	double c = 2 * atan2(sqrt(d), sqrt(1-d));
	return earth_radius_km * c;
}


Client::Client() {
	Icon(Images::icon());
	Title("F2F Client program");
	Sizeable().MaximizeBox().MinimizeBox();
	
	AddFrame(menu);
	menu.Set(THISBACK(MainMenu));
	
	Add(split.SizePos());
	split << irc << rvsplit;
	split.Horz();
	split.SetPos(5555);
	
	rvsplit << map << rhsplit;
	rvsplit.Vert();
	rvsplit.SetPos(6666);
	rhsplit << nearestlist << details;
	rhsplit.Horz();
	rhsplit.SetPos(2500);
	
	CtrlLayout(details);
	
	irc.WhenCommand << THISBACK(Command);
	irc.WhenChannelChanged << THISBACK(RefreshGuiChannel);
	
	map.Set(Pointf(25.46748, 65.05919));
	map.WhenHome << THISBACK(ChangeLocation);
	
	nearestlist.AddIndex();
	nearestlist.AddColumn("Distance (km)");
	nearestlist.AddColumn("Name");
	nearestlist <<= THISBACK(RefreshNearest);
	
	details.channels.AddColumn("Channel");
}

Client::~Client() {
	running = false;
	if (!s.IsEmpty()) s->Close();
	while (!stopped) Sleep(100);
}

void Client::MainMenu(Bar& bar) {
	bar.Sub("File", [=](Bar& bar) {
		bar.Add("Join a channel", THISBACK(JoinChannel));
	});
	bar.Sub("Help", [=](Bar& bar) {
		
	});
	
}

bool Client::Connect() {
	if (s.IsEmpty() || !s->IsOpen()) {
		if (!s.IsEmpty()) s.Clear();
		s.Create();
		
		if(!s->Connect(addr, port)) {
			Print("Client " + IntStr(user_id) + " Unable to connect to server!");
			return false;
		}
	}
	return true;
}

void Client::Disconnect() {
	s.Clear();
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
			ChangeLocation(map.Get());
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

void Client::SetAge(int i) {
	if (age == i) return;
	try {
		if (Set("age", IntStr(i)))
			age = i;
	}
	catch (Exc e) {
		Print("Changing age failed");
	}
}

void Client::SetGender(bool b) {
	if (gender == b) return;
	try {
		if (Set("gender", IntStr(b)))
			gender = b;
	}
	catch (Exc e) {
		Print("Changing gender failed");
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
		
		if (continuous)
			Connect();
		
		try {
			while (!Thread::IsShutdownThreads() && running) {
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
		
		is_logged_in = false;
		
		if (continuous)
			Disconnect();
	}
	
	Print("Client " + IntStr(user_id) + " Stopping");
	stopped = true;
}

void Client::Call(Stream& out, Stream& in) {
	int r;
	
	out.Seek(0);
	String out_str = out.Get(out.GetSize());
	int out_size = out_str.GetCount();
	
	call_lock.Enter();
	
	if (!continuous) {
		s.Clear();
		Connect();
	}
	
	r = s->Put(&out_size, sizeof(out_size));
	if (r != sizeof(out_size)) {if (!continuous) Disconnect(); call_lock.Leave(); throw Exc("Data sending failed");}
	r = s->Put(out_str.Begin(), out_str.GetCount());
	if (r != out_str.GetCount()) {if (!continuous) Disconnect(); call_lock.Leave(); call_lock.Leave(); throw Exc("Data sending failed");}
	
	s->Timeout(30000);
	int in_size;
	r = s->Get(&in_size, sizeof(in_size));
	if (r != sizeof(in_size) || in_size < 0 || in_size >= 10000000) {if (!continuous) Disconnect(); call_lock.Leave(); call_lock.Leave(); throw Exc("Received invalid size");}
	
	String in_data = s->Get(in_size);
	if (in_data.GetCount() != in_size) {if (!continuous) Disconnect(); call_lock.Leave(); call_lock.Leave(); throw Exc("Received invalid data");}
	
	if (!continuous)
		Disconnect();
	
	call_lock.Leave();
	
	int64 pos = in.GetPos();
	in << in_data;
	in.Seek(pos);
}

void Client::Register() {
	StringStream out, in;
	
	out.Put32(10);
	
	Call(out, in);
	
	in.Get(&user_id, sizeof(int));
	pass = in.Get(8);
	if (pass.GetCount() != 8) throw Exc("Invalid password");
	
	Print("Client " + IntStr(user_id) + " registered (pass " + pass + ")");
}

void Client::Login() {
	StringStream out, in;
	
	out.Put32(20);
	out.Put32(user_id);
	out.Put(pass.Begin(), pass.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Login failed");
	
	login_id = in.Get64();
	
	int name_len = in.Get32();
	if (name_len <= 0) throw Exc("Login failed");
	user_name = in.Get(name_len);
	
	age = in.Get32();
	gender = in.Get32();
	
	Print("Client " + IntStr(user_id) + " logged in (" + IntStr(user_id) + ", " + pass + ") nick: " + user_name);
}

bool Client::Set(const String& key, const String& value) {
	StringStream out, in;
	
	out.Put32(30);
	
	out.Put64(login_id);
	
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
	
	out.Put32(40);
	
	out.Put64(login_id);
	
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
	
	out.Put32(50);
	
	out.Put64(login_id);
	
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
	
	out.Put32(60);
	
	out.Put64(login_id);
	
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
	
	out.Put32(70);
	
	out.Put64(login_id);
	
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
	
	out.Put32(80);
	
	out.Put64(login_id);
	
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
			PostCallback(THISBACK(RefreshGui));
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
			PostCallback(THISBACK(RefreshGui));
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
	PostCallback(THISBACK(RefreshGui));
}

void Client::SendLocation(const Location& l) {
	StringStream out, in;
	
	out.Put32(90);
	
	out.Put64(login_id);
	
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
	
	out.Put32(100);
	
	out.Put64(login_id);
	
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
	u.age = in.Get32();
	u.gender = in.Get32();
	
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
		
		Pointf my_position = map.Get();
		
		int count = ch.userlist.GetCount();
		irc.SetUserCount(count);
		map.SetPersonCount(count);
		count = min(count, ch.userlist.GetCount());
		for(int i = 0; i < count; i++) {
			int user_id = ch.userlist[i];
			User& u = users.GetAdd(user_id);
			irc.SetUser(i, u.profile_image, u.name);
			Pointf user_pt(u.longitude, u.latitude);
			map.SetPerson(i, user_pt, u.profile_image);
			
			double distance = CoordinateDistanceKM(my_position, user_pt);
			nearestlist.Set(i, 0, user_id);
			nearestlist.Set(i, 1, distance);
			nearestlist.Set(i, 2, u.name);
		}
		nearestlist.SetCount(count);
		nearestlist.SetSortColumn(0, false);
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

void Client::RefreshNearest() {
	int i = nearestlist.GetCursor();
	if (i < 0 || i >= nearestlist.GetCount()) return;
	int user_id = nearestlist.Get(i, 0);
	User& u = users.GetAdd(user_id);
	details.profile_img.SetImage(u.profile_image);
	details.name.SetLabel(u.name);
	details.age.SetLabel(IntStr(u.age));
	details.gender.SetLabel(u.gender ? "Male" : "Female");
	for(int i = 0; i < u.channels.GetCount(); i++) {
		details.channels.Set(i, 0, u.channels[i]);
	}
	details.channels.SetCount(u.channels.GetCount());
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
				is_logged_in = false;
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
				is_logged_in = false;
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
				is_logged_in = false;
			}
		}
		PostCallback(THISBACK(RefreshGui));
	}
}

void Client::JoinChannel() {
	String channels_data;
	Get("allchannellist", channels_data);
	MemReadStream data(channels_data.Begin(), channels_data.GetCount());
	
	WithAllChannels<TopWindow> win;
	CtrlLayoutOKCancel(win, "All channels");
	win.chlist.AddColumn("Channel");
	win.chlist.AddColumn("User count");
	win.chlist.ColumnWidths("4 1");
	win.chlist.WhenLeftDouble << Proxy(win.ok.WhenAction);
	
	int ch_count = data.Get32();
	for(int i = 0; i < ch_count; i++) {
		int ch_len = data.Get32();
		String ch = data.Get(ch_len);
		int user_count = data.Get32();
		
		win.chlist.Set(i, 0, ch);
		win.chlist.Set(i, 1, user_count);
	}
	win.chlist.SetSortColumn(0, false);
	if (win.Execute() == IDOK) {
		int cursor = win.chlist.GetCursor();
		if (cursor >= 0 && cursor < win.chlist.GetCount())
			Join(win.chlist.Get(cursor, 0));
	}
}










void Channel::Post(int user_id, String user_name, const String& msg) {
	lock.Enter();
	ChannelMessage& m = messages.Add();
	m.received = GetSysTime();
	m.message = msg;
	m.sender_id = user_id;
	m.sender_name = user_name;
	unread++;
	lock.Leave();
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
	for(int i = buttons.GetCount()-1; i >= count; i--) {
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












int PasswordFilter(int c) {
	return '*';
}

ServerDialog::ServerDialog(Client& c) : cl(c), sd(*this), rd(*this) {
	CtrlLayout(*this, "F2F select server");
	
	password.SetFilter(PasswordFilter);
	
	LoadThis();
	
	addr.SetData(srv_addr);
	port.SetData(srv_port);
	auto_connect.Set(autoconnect);
	
	reg <<= THISBACK(Register);
	selectserver <<= THISBACK(SelectServer);
	
	addr.SetFocus();
	Enable(true);
}

void ServerDialog::Enable(bool b) {
	reg.Enable(b);
	username.Enable(b);
	password.Enable(b);
	addr.Enable(b);
	port.Enable(b);
	auto_connect.Enable(b);
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

void ServerDialog::TryConnect() {
	autoconnect = auto_connect.Get();
	srv_addr = addr.GetData();
	srv_port = port.GetData();
	StoreThis();
	
	if (srv_addr.IsEmpty())
		PostCallback(THISBACK1(SetError, "Server address is empty"));
	else if (Connect(true))
		PostCallback(THISBACK(Close0));
	else
		PostCallback(THISBACK1(SetError, "Connecting server failed"));
	PostCallback(THISBACK1(Enable, true));
}

bool ServerDialog::Connect(bool do_login) {
	srv_addr = addr.GetData();
	srv_port = port.GetData();
	cl.SetAddress(srv_addr, srv_port);
	cl.LoadThis();
	cl.CloseConnection();
	return cl.Connect() && cl.RegisterScript() && (!do_login || cl.LoginScript());
}

void ServerDialog::StopConnect() {
	cl.CloseConnection();
}

void ServerDialog::Register() {
	rd.Register();
	if (rd.Execute() == IDOK) {
		username.SetData(rd.username.GetData());
		password.SetData(rd.password.GetData());
		StoreThis();
	}
}

void ServerDialog::SelectServer() {
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













SettingsDialog::SettingsDialog(Client& cl) : cl(cl) {
	CtrlLayoutOKCancel(*this, "F2F settings");
	
	genderctrl.Add("Female");
	genderctrl.Add("Male");
	LoadThis();
	
	if (profile_image.IsEmpty() && FileExists(ConfigFile("profile_image_none.png")))
		profile_image = StreamRaster::LoadFileAny(ConfigFile("profile_image_none.png"));
	
	agectrl.SetData(age);
	genderctrl.SetIndex(gender);
	image.SetImage(profile_image);
	namectrl.SetData(name);
	
	selectimage <<= THISBACK(SelectImage);
	
	namectrl.SetFocus();
}

void SettingsDialog::SelectImage() {
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

bool SettingsDialog::Setup() {
	name = namectrl.GetData();
	age = agectrl.GetData();
	gender = genderctrl.GetIndex();
	is_first_start = false;
	
	if (profile_image.GetSize() == Size(0,0)) {
		PromptOK("Profile image is not set");
		return false;
	}
	else if (name.IsEmpty()) {
		PromptOK("Profile name is empty");
		return false;
	}
	
	cl.SetName(name);
	cl.SetAge(age);
	cl.SetGender(gender);
	cl.SetImage(profile_image);
	
	StoreThis();
	return true;
}










RegisterDialog::RegisterDialog(ServerDialog& sd) : sd(sd) {
	CtrlLayoutOKCancel(*this, "Register to server");
}

RegisterDialog::~RegisterDialog() {
	
}
	
void RegisterDialog::Register() {
	Thread::Start(THISBACK(TryRegister));
}

void RegisterDialog::TryRegister() {
	if (sd.Connect(false)) {
		GuiLock __;
		username.SetData(sd.cl.GetUserId());
		password.SetData(sd.cl.GetPassword());
	}
}
	
	
	
	
	
	
	
	
	
	
	
SelectServerDialog::SelectServerDialog(ServerDialog& sd) : sd(sd) {
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
	serverlist.WhenLeftDouble << Proxy(ok.WhenAction);
	
	Thread::Start(THISBACK(RefreshAddresses));
}

void SelectServerDialog::RefreshAddresses() {
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

void SelectServerDialog::TestConnection(int i) {
	Server& server = servers[i];
	TcpSocket& sock = socks[i];
	
	try {
		sock.Timeout(10000);
		
		if (!sock.Connect(server.addr, server.port))
			throw Exc("Couldn't connect " + server.addr + ":" + IntStr(server.port));
		
		TimeStop ts;
		
		StringStream ss;
		int greeting_cmd = 0;
		ss.Put(&greeting_cmd, sizeof(int));
		
		ss.Seek(0);
		String data = ss.Get(ss.GetSize());
		
		int r, datalen = data.GetCount();
		r = sock.Put(&datalen, sizeof(int));
		if (r != sizeof(int)) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		r = sock.Put(data.Begin(), datalen);
		if (r != datalen) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		
		ss.Seek(0);
		r = sock.Get(&datalen, sizeof(int));
		if (r != sizeof(int)) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		ss << sock.Get(datalen);
		ss.Seek(0);
		
		int greet_len;
		r = ss.Get(&greet_len, sizeof(int));
		if (r != sizeof(int) || greet_len < 0 || greet_len > 1000) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		server.greeting = ss.Get(greet_len);
		
		r = ss.Get(&server.sessions, sizeof(int));
		if (r != sizeof(int)) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		r = ss.Get(&server.max_sessions, sizeof(int));
		if (r != sizeof(int)) throw Exc("Connection failed " + server.addr + ":" + IntStr(server.port));
		
		server.elapsed = ts.Elapsed();
	}
	catch (Exc e) {
		Print(e);
	}
	
	PostCallback(THISBACK(Data));
	running--;
}

void SelectServerDialog::Data() {
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


