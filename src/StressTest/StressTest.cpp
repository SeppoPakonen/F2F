#include "StressTest.h"
#include "AES.h"

Client::Client() {
	
}
	
void Client::Run() {
	Cout() << "Client " << id << " Running\n";
	
	int count = 0;
	
	while (!Thread::IsShutdownThreads()) {
		if (!s.IsEmpty()) s.Clear();
		s.Create();
		
		if(!s->Connect("127.0.0.1", 17000)) {
			Cout() << "Client " << id << " Unable to connect to server!\n";
			return;
		}
		
		try {
			while (!Thread::IsShutdownThreads()) {
				
				int action;
				if (count == 0) {
					action = 100;
				}
				else if (count == 1) {
					action = 200;
				}
				else {
					double p = Randomf();
					if (p < 0.1)		action = 300;
					else if (p < 0.2)	action = 400;
					else if (p < 0.3)	action = 500;
					else if (p < 0.6)	action = 600;
					else if (p < 0.7)	action = 700;
					else if (p < 0.8)	action = 800;
					else				action = 900;
				}
				
				switch (action) {
					case 100:		Register(); break;
					case 200:		Login();
									RefreshUserlist();
									break;
					//case 300:		Set(); break;
					//case 400:		Get(); break;
					case 500:		Join(RandomNewChannel()); break;
					case 600:		Leave(RandomOldChannel()); break;
					case 700:		Message(RandomUser(), RandomMessage()); break;
					case 800:		Poll(); break;
					case 900:		SendLocation(RandomLocation()); break;
				}
				
				Sleep(Random(1000));
				count++;
			}
		}
		catch (Exc e) {
			Cout() << "Client " << id << " Error: " << e << "\n";
			count = min(count, 1);
		}
		catch (const char* e) {
			Cout() << "Client " << id << " Error: " << e << "\n";
			count = min(count, 1);
		}
		catch (...) {
			Cout() << "Client " << id << " Unexpected error\n";
			break;
		}
	}
	
	
	Cout() << "Client " << id << " Stopping\n";
}

void Client::Call(Stream& out, Stream& in) {
	int r;
	
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
	
	s->Timeout(5000);
	int in_size;
	r = s->Get(&in_size, sizeof(in_size));
	if (r != sizeof(in_size) || in_size < 0 || in_size >= 100000) throw Exc("Received invalid size");
	
	String in_data = s->Get(in_size);
	if (in_data.GetCount() != in_size) throw Exc("Received invalid data");
	
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
	if (pass.GetCount() != 8) throw Exc("Invalid  password");
	
	Cout() << "Client " << id << " registered (pass " << pass << ")\n";
}

void Client::Login() {
	StringStream out, in;
	
	out.Put32(200);
	out.Put32(user_id);
	out.Put(pass.Begin(), pass.GetCount());
	
	Call(out, in);
	
	sesspass = in.Get(8);
	if (sesspass.GetCount() != 8) throw Exc("Invalid session password");
	
	Cout() << "Client " << id << " logged in (session password " << sesspass << ")\n";
}

bool Client::Set(const String& key, const String& value) {
	StringStream out, in;
	
	out.Put32(300);
	out.Put(sesspass.Begin(), sesspass.GetCount());
	
	out.Put32(key.GetCount());
	out.Put(key.Begin(), key.GetCount());
	out.Put32(value.GetCount());
	out.Put(value.Begin(), value.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret == 1) {
		Cout() << "Client " << id << " set " << key << " = " << value << " FAILED\n";
		return 1;
	}
	else if (ret != 0) throw Exc("Setting value failed");
	
	Cout() << "Client " << id << " set " << key << " = " << value << "\n";
	return 0;
}

void Client::Get(const String& key, String& value) {
	StringStream out, in;
	
	out.Put32(400);
	out.Put(sesspass.Begin(), sesspass.GetCount());
	
	out.Put32(key.GetCount());
	out.Put(key.Begin(), key.GetCount());
	
	Call(out, in);
	
	int value_len = in.Get32();
	value = in.Get(value_len);
	if (value.GetCount() != value_len) throw Exc("Getting value failed");
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Getting value failed");
	
	Cout() << "Client " << id << " get " << key << "\n";
}

void Client::Join(String channel) {
	if (channel.IsEmpty()) return;
	StringStream out, in;
	
	out.Put32(500);
	out.Put(sesspass.Begin(), sesspass.GetCount());
	
	int ch_len = channel.GetCount();
	out.Put32(ch_len);
	out.Put(channel.Begin(), channel.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Joining channel failed");
	
	joined_channels.Add(channel);
	
	Cout() << "Client " << id << " joined channel " << channel << "\n";
}

void Client::Leave(String channel) {
	if (channel.IsEmpty()) return;
	StringStream out, in;
	
	out.Put32(600);
	out.Put(sesspass.Begin(), sesspass.GetCount());
	
	int ch_len = channel.GetCount();
	out.Put32(ch_len);
	out.Put(channel.Begin(), channel.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Leaving channel failed");
	
	joined_channels.RemoveKey(channel);
	
	Cout() << "Client " << id << " left from channel " << channel << "\n";
}

void Client::Message(int recv_user_id, const String& msg) {
	if (recv_user_id < 0) return;
	StringStream out, in;
	
	out.Put32(700);
	out.Put(sesspass.Begin(), sesspass.GetCount());
	
	out.Put32(recv_user_id);
	out.Put32(msg.GetCount());
	out.Put(msg.Begin(), msg.GetCount());
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Message sending failed");
	
	Cout() << "Client " << id << " sent message from " << user_id << " to " << recv_user_id << ": " << msg << "\n";
}

void Client::Poll() {
	StringStream out, in;
	
	out.Put32(800);
	out.Put(sesspass.Begin(), sesspass.GetCount());
	
	Call(out, in);
	
	int count = in.Get32();
	if (count < 0 || count >= 10000) throw Exc("Polling failed");
	for(int i = 0; i < count; i++) {
		int sender_id = in.Get32();
		int msg_len = in.Get32();
		String message = in.Get(msg_len);
		if (message.GetCount() != msg_len) throw Exc("Polling failed");
		Cout() << "Client " << id << " received from " << sender_id << ": " << message << "\n";
	}
}

void Client::SendLocation(const Location& l) {
	StringStream out, in;
	
	out.Put32(900);
	out.Put(sesspass.Begin(), sesspass.GetCount());
	
	out.Put(&l.latitude, sizeof(l.latitude));
	out.Put(&l.longitude, sizeof(l.longitude));
	out.Put(&l.elevation, sizeof(l.elevation));
	
	Call(out, in);
	
	int ret = in.Get32();
	if (ret != 0) throw Exc("Updating location failed");
	
	Cout() << "Client " << id << " updated location\n";
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
		if (name_len <= 0) continue;
		String name = in.Get(name_len);
		
		User& u = users.GetAdd(name);
		u.user_id = user_id;
		u.name = name;
		if (u.name.GetCount() != name_len) fail = true;
	}
	if (fail) throw Exc("Getting userlist failed");
	
	Cout() << "Client " << id << " updated userlist (size " << user_count << ")\n";
}

String Client::RandomNewChannel() {
	switch (Random(11)) {
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
	return users[Random(users.GetCount())].user_id;
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


CONSOLE_APP_MAIN
{
	Array<Client> clients;
	
	for(int i = 0; i < 10; i++) {
		Client& c = clients.Add();
		c.SetId(i);
		c.Start();
	}
	
	
	while (!Thread::IsShutdownThreads()) Sleep(100);
}
