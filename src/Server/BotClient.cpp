#include "Server.h"
#include <plugin/jpg/jpg.h>

Client::Client() {
	location = RandomLocation();
	radius = 0.15;
	step = 2 * M_PI / 20;
}

void Client::Run() {
	Print("Client " + IntStr(id) + " Running");
	
	int count = 0;
	bool registered = false;
	
	while (!Thread::IsShutdownThreads() && running) {
		if (!s.IsEmpty()) s.Clear();
		s.Create();
		
		if(!s->Connect("127.0.0.1", 17000)) {
			Print("Client " + IntStr(id) + " Unable to connect to server!");
			return;
		}
		
		bool logged_in = false;
		
		try {
			int pre_cmd = -1;
			int r = s->Put(&pre_cmd, sizeof(int));
			if (r != sizeof(int)) throw Exc("Precommand skipping failed");
			
			while (!Thread::IsShutdownThreads() && s->IsOpen() && running) {
				
				int action;
				if (!registered) {
					action = 100;
				}
				else if (!logged_in) {
					action = 200;
				}
				else {
					switch (Random(10)) {
						case 0:	action = 301;	break;
						case 1:	action = 400;	break;
						case 2:
						case 3:	action = 500;	break;
						case 4:	action = 600;	break;
						case 5:	action = 700;	break;
						case 6:	action = 800;	break;
						case 7:	action = 900;	break;
						case 8:	action = 1000;	break;
						case 9:	action = 302;	break;
					}
				}
				
				switch (action) {
					case 100:		Register(); registered = true; break;
					case 200:		Login();
									RefreshChannellist();
									RefreshUserlist();
									Set("profile_image", RandomImage());
									Set("age", IntStr(18 + Random(100)));
									Set("gender", IntStr(Random(2)));
									logged_in = true;
									break;
					case 301:		Set("name", RandomName()); break;
					case 302:		Set("profile_image", RandomImage()); break;
					case 500:		Join(RandomNewChannel()); break;
					case 600:		Leave(RandomOldChannel()); break;
					case 700:		Message(RandomUser(), RandomMessage()); break;
					case 800:		Poll(); break;
					case 900:		SendLocation(NextLocation()); break;
					case 1000:		ChannelMessage(RandomOldChannel(), RandomMessage()); break;
				}
				
				Sleep(Random(1000));
				count++;
			}
		}
		catch (Exc e) {
			Print("Client " + IntStr(id) + " Error: " + e);
		}
		catch (const char* e) {
			Print("Client " + IntStr(id) + " Error: " + e);
		}
		catch (...) {
			Print("Client " + IntStr(id) + " Unexpected error");
			break;
		}
	}
	
	Print("Client " + IntStr(id) + " Stopping");
	stopped = true;
}

void Client::Call(Stream& out, Stream& in) {
	int r;
	
	out.Seek(0);
	String out_str = out.Get(out.GetSize());
	int out_size = out_str.GetCount();
	
	r = s->Put(&out_size, sizeof(out_size));
	if (r != sizeof(out_size)) throw Exc("Data sending failed");
	r = s->Put(out_str.Begin(), out_str.GetCount());
	if (r != out_str.GetCount()) throw Exc("Data sending failed");
	
	s->Timeout(30000);
	int in_size;
	r = s->Get(&in_size, sizeof(in_size));
	if (r != sizeof(in_size) || in_size < 0 || in_size >= 10000000) throw Exc("Received invalid size");
	
	String in_data = s->Get(in_size);
	if (in_data.GetCount() != in_size) throw Exc("Received invalid data");
	
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
	if (pass.GetCount() != 8) throw Exc("Invalid  password");
	
	Print("Client " + IntStr(id) + " registered (pass " + pass + ")");
}

void Client::Login() {
	StringStream out, in;
	
	out.Put32(20);
	out.Put32(user_id);
	out.Put(pass.Begin(), pass.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Login failed");
	
	int name_len = in.Get32();
	user_name = in.Get(name_len);
	
	age = in.Get32();
	gender = in.Get32();
	
	Print("Client " + IntStr(id) + " logged in (" + IntStr(user_id) + ", " + pass + ") nick: " + user_name);
}

bool Client::Set(const String& key, const String& value) {
	StringStream out, in;
	
	out.Put32(30);
	
	out.Put32(key.GetCount());
	out.Put(key.Begin(), key.GetCount());
	out.Put32(value.GetCount());
	out.Put(value.Begin(), value.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret == 1) {
		//Print("Client " + IntStr(id) + " set " + key + " = " + value + " FAILED");
		return 1;
	}
	else if (ret != 0) throw Exc("Setting value failed");
	
	//Print("Client " + IntStr(id) + " set " + key + " = " + value);
	return 0;
}

void Client::Get(const String& key, String& value) {
	StringStream out, in;
	
	out.Put32(40);
	
	out.Put32(key.GetCount());
	out.Put(key.Begin(), key.GetCount());
	
	Call(out, in);
	
	int value_len = in.Get32();
	value = in.Get(value_len);
	if (value.GetCount() != value_len) throw Exc("Getting value failed");
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Getting value failed");
	
	Print("Client " + IntStr(id) + " get " + key);
}

void Client::Join(String channel) {
	if (channel.IsEmpty()) return;
	StringStream out, in;
	
	out.Put32(50);
	
	int ch_len = channel.GetCount();
	out.Put32(ch_len);
	out.Put(channel.Begin(), channel.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret == 1) {
		Print("Client " + IntStr(id) + " WAS JOINED ALREADY AT channel " + channel);
		return;
	}
	
	if (ret != 0) throw Exc("Joining channel failed");
	
	joined_channels.Add(channel);
	
	Print("Client " + IntStr(id) + " joined channel " + channel);
}

void Client::Leave(String channel) {
	if (channel.IsEmpty()) return;
	StringStream out, in;
	
	out.Put32(60);
	
	int ch_len = channel.GetCount();
	out.Put32(ch_len);
	out.Put(channel.Begin(), channel.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Leaving channel failed");
	
	joined_channels.RemoveKey(channel);
	
	Print("Client " + IntStr(id) + " left from channel " + channel);
}

bool Client::Message(int recv_user_id, const String& msg) {
	if (recv_user_id < 0) return false;
	StringStream out, in;
	
	out.Put32(70);
	
	out.Put32(recv_user_id);
	out.Put32(msg.GetCount());
	out.Put(msg.Begin(), msg.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret == 0)
		Print("Client " + IntStr(id) + " sent message from " + IntStr(user_id) + " to " + IntStr(recv_user_id) + ": " + msg);
	return !ret;
}

void Client::Poll() {
	StringStream out, in;
	
	out.Put32(80);
	
	Call(out, in);
	
	int count = in.Get32();
	if (count < 0 || count >= 10000) throw Exc("Polling failed");
	for(int i = 0; i < count; i++) {
		int sender_id = in.Get32();
		int msg_len = in.Get32();
		String message;
		if (msg_len > 0)
			message = in.Get(msg_len);
		int recv_len = message.GetCount();
		bool is_eof = in.IsEof();
		if (recv_len != msg_len) throw Exc("Polling failed");
		//Print("Client " + IntStr(id) + " received from " + IntStr(sender_id) + ": " + IntStr(message.GetCount()));
		
		int j = message.Find(" ");
		if (j == -1) continue;
		String key = message.Left(j);
		message = message.Mid(j + 1);
		
		if (key == "msg") {
			WhenMessage(sender_id, message);
		}
		else if (key == "chmsg") {
			
		}
		else if (key == "join") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 2) throw Exc("Polling argument error");
			int user_id = StrInt(args[0]);
			String channel = args[1];
			if (user_id == this->user_id) // mysterious bug
				continue;
			ASSERT(user_id != this->user_id && user_id >= 0);
			User& u = users.GetAdd(user_id);
			u.user_id = user_id;
			u.channels.Add(channel);
		}
		else if (key == "leave") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 2) throw Exc("Polling argument error");
			int user_id = StrInt(args[0]);
			String channel = args[1];
			if (user_id == this->user_id) // mysterious bug
				continue;
			ASSERT(user_id != this->user_id);
			User& u = users.GetAdd(user_id);
			u.user_id = user_id;
			u.channels.RemoveKey(channel);
			if (u.channels.IsEmpty())
				users.RemoveKey(user_id);
		}
		else if (key == "name") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 2) throw Exc("Polling argument error");
			int user_id = StrInt(args[0]);
			if (user_id == this->user_id) // mysterious bug
				continue;
			String user_name = args[1];
			ASSERT(user_id != this->user_id);
			User& u = users.GetAdd(user_id);
			u.name = user_name;
			u.user_id = user_id;
		}
		else if (key == "profile") {
			
		}
		else if (key == "loc") {
			
		}
		else Panic("Unhandled key: " + key);
	}
}

void Client::SendLocation(const Location& l) {
	StringStream out, in;
	
	out.Put32(90);
	
	out.Put(&l.latitude, sizeof(l.latitude));
	out.Put(&l.longitude, sizeof(l.longitude));
	out.Put(&l.elevation, sizeof(l.elevation));
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Updating location failed");
	
	Print("Client " + IntStr(id) + " updated location");
}

bool Client::ChannelMessage(String channel, const String& msg) {
	if (channel.IsEmpty()) return false;
	StringStream out, in;
	
	out.Put32(100);
	
	out.Put32(channel.GetCount());
	out.Put(channel.Begin(), channel.GetCount());
	out.Put32(msg.GetCount());
	out.Put(msg.Begin(), msg.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (!ret)
		Print("Client " + IntStr(id) + " sent message from " + IntStr(user_id) + " to " + channel + ": " + msg);
	return !ret;
}

void Client::RefreshChannellist() {
	String channellist_str;
	Get("channellist", channellist_str);
	MemReadStream in(channellist_str.Begin(), channellist_str.GetCount());
	
	Index<String> ch_rem;
	for(int i = 0; i < joined_channels.GetCount(); i++) ch_rem.Add(joined_channels[i]);
	
	int ch_count = in.Get32();
	bool fail = false;
	for(int i = 0; i < ch_count; i++) {
		int name_len = in.Get32();
		if (name_len <= 0) continue;
		String name = in.Get(name_len);
		
		if (ch_rem.Find(name) != -1) ch_rem.RemoveKey(name);
		
		joined_channels.FindAdd(name);
	}
	if (fail) throw Exc("Getting channellist failed");
	
	for(int i = 0; i < ch_rem.GetCount(); i++)
		joined_channels.RemoveKey(ch_rem[i]);
}

void Client::RefreshUserlist() {
	String userlist_str;
	Get("userlist", userlist_str);
	MemReadStream in(userlist_str.Begin(), userlist_str.GetCount());
	
	int user_count = in.Get32();
	bool fail = false;
	for(int i = 0; i < user_count; i++) {
		int user_id = in.Get32();
		int name_len = in.Get32();
		if (name_len <= 0 || user_id < 0) continue;
		String name = in.Get(name_len);
		
		ASSERT(user_id != this->user_id);
		User& u = users.GetAdd(user_id);
		u.user_id = user_id;
		u.name = name;
		if (u.name.GetCount() != name_len) fail = true;
		in.Get(&u.age, sizeof(int));
		in.Get(&u.gender, sizeof(int));
		in.Get(&u.profile_img_hash, sizeof(int));
		in.Get(&u.longitude, sizeof(double));
		in.Get(&u.latitude, sizeof(double));
		in.Get(&u.elevation, sizeof(double));
		
		int channel_count = in.Get32();
		if (channel_count < 0 || channel_count >= 200) {fail = true; continue;}
		for(int j = 0; j < channel_count; j++) {
			int ch_len = in.Get32();
			String ch_name = in.Get(ch_len);
			u.channels.Add(ch_name);
		}
	}
	if (fail) throw Exc("Getting userlist failed");
	
	Print("Client " + IntStr(id) + " updated userlist (size " + IntStr(user_count) + ")");
}

String Client::RandomImage() {
	ImageBuffer img(128, 128);
	Fill(img, Size(128, 128), Color(Random(255), Random(255), Random(255)));
	JPGEncoder jpg;
	return jpg.SaveString(img);
}

String Client::RandomName() {
	String s;
	for(int i = 0; i < 8; i++)
		s.Cat('A' + Random(25));
	return s;
	return s;
}

String Client::RandomNewChannel() {
	switch (Random(11 + 1)) {
		case 0: return "sports";
		case 1: return "animals";
		case 2: return "news";
		case 3: return "education";
		case 4: return "finance";
		case 5: return "IT";
		case 6: return "fun";
		case 7: return "social";
		case 8: return "alt";
		case 9: return "travel";
		case 10: return "testers";
	}
	String s;
	for(int i = 0; i < 8; i++)
		s.Cat('A' + Random(25));
	return s;
}

String Client::RandomOldChannel() {
	if (joined_channels.IsEmpty()) return "";
	return joined_channels[Random(joined_channels.GetCount())];
}

int Client::RandomUser() {
	if (users.IsEmpty()) return user_id;
	int id = users[Random(users.GetCount())].user_id;
	ASSERT(id != this->user_id);
	return id;
}

String Client::RandomMessage() {
	String msg;
	int words = Random(10);
	for(int i = 0; i < words; i++) {
		if (i) msg.Cat(' ');
		switch (Random(10)) {
			case 0: msg += "apples"; break;
			case 1: msg += "is"; break;
			case 2: msg += "oranges"; break;
			case 3: msg += "while"; break;
			case 4: msg += "black"; break;
			case 5: msg += "have"; break;
			case 6: msg += "white"; break;
			case 7: msg += "in"; break;
			case 8: msg += "at"; break;
			case 9: msg += "yes"; break;
		}
	}
	return msg;
}

Location Client::RandomLocation() {
	Location l;
	double left = 25.4630;
	double top = 65.0618;
	double right = 25.4724;
	double bottom = 65.0565;
	double height = 5.0;
	l.longitude = left + Randomf() * (right - left);
	l.latitude = bottom + Randomf() * (top - bottom);
	l.elevation = Randomf() * height;
	return l;
}

Location Client::NextLocation() {
	double left = 25.4630;
	double top = 65.0618;
	double right = 25.4724;
	double bottom = 65.0565;
	double width = right - left;
	double height = top - bottom;
	Location l = location;
	steps += 1;
	double cx = cos(steps * step) * radius * width;
	double cy = sin(steps * step) * radius * height;
	l.longitude += cx;
	l.latitude += cy;
	return l;
}

