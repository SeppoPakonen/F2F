#include "Client.h"
#include "AES.h"


void Print(const String& s) {
	static Mutex lock;
	lock.Enter();
	Cout() << s;
	Cout().PutEol();
	lock.Leave();
}



Client::Client() {
	Icon(Images::icon());
	Title("F2F Client program");
	Sizeable().MaximizeBox().MinimizeBox();
	
	Add(irc.SizePos());
	
	irc.WhenCommand << THISBACK(Command);
	irc.WhenChannelChanged << THISBACK(RefreshGuiChannel);
}


void Client::Connect() {
	Print("Client " + IntStr(user_id) + " Running");
	
	int count = 0;
	
	while (!Thread::IsShutdownThreads()) {
		if (!s.IsEmpty()) s.Clear();
		s.Create();
		
		if(!s->Connect("127.0.0.1", 17000)) {
			Print("Client " + IntStr(user_id) + " Unable to connect to server!");
			return;
		}
		
		try {
			while (!Thread::IsShutdownThreads() && s->IsOpen()) {
				
				
				if (!is_registered) {
					Register();
					is_registered = true;
				}
				else if (!is_logged_in) {
					Login();
					RefreshChannellist();
					RefreshUserlist();
					is_logged_in = true;

					PostCallback(THISBACK(RefreshGui));
				}
				else {
					Poll();
					Sleep(1000);
					count++;
				}
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
	}
	
	
	Print("Client " + IntStr(user_id) + " Stopping");
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
	
	s->Timeout(30000);
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
		Print("Client " + IntStr(user_id) + " set " + key + " = " + value + " FAILED");
		return 1;
	}
	else if (ret != 0) throw Exc("Setting value failed");
	
	Print("Client " + IntStr(user_id) + " set " + key + " = " + value);
	return 0;
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
	
	channels.RemoveKey(channel);
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
	
	int count = in.Get32();
	if (count < 0 || count >= 10000) throw Exc("Polling failed");
	for(int i = 0; i < count; i++) {
		int sender_id = in.Get32();
		int msg_len = in.Get32();
		String message;
		if (msg_len > 0)
			message = in.Get(msg_len);
		if (message.GetCount() != msg_len) throw Exc("Polling failed");
		Print("Client " + IntStr(user_id) + " received from " + IntStr(sender_id) + ": " + message);
		
		int j = message.Find(" ");
		if (j == -1) continue;
		String key = message.Left(j);
		message = message.Mid(j + 1);
		
		if (key == "msg") {
			String ch_name = "user" + IntStr(sender_id);
			ASSERT(sender_id != this->user_id);
			User& u = users.GetAdd(sender_id);
			Channel& ch = channels.GetAdd(ch_name);
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
			if (args.GetCount() != 3) throw Exc("Polling argument error");
			String user_name = args[0];
			int user_id = StrInt(args[1]);
			ASSERT(user_id != this->user_id);
			String ch_name = args[2];
			User& u = users.GetAdd(user_id);
			u.name = user_name;
			u.user_id = user_id;
			u.channels.FindAdd(ch_name);
			Channel& ch = channels.GetAdd(ch_name);
			ch.userlist.FindAdd(user_id);
			ch.Post(-1, "Server", Format("User %s joined channel %s", user_name, ch_name));
			PostCallback(THISBACK(RefreshGui));
		}
		else if (key == "leave") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 3) throw Exc("Polling argument error");
			String user_name = args[0];
			int user_id = StrInt(args[1]);
			ASSERT(user_id != this->user_id);
			String ch_name = args[2];
			User& u = users.GetAdd(user_id);
			u.name = user_name;
			u.user_id = user_id;
			u.channels.RemoveKey(ch_name);
			if (u.channels.IsEmpty())
				users.RemoveKey(user_id);
			Channel& ch = channels.GetAdd(ch_name);
			ch.userlist.RemoveKey(user_id);
			ch.Post(-1, "Server", Format("User %s left channel %s", user_name, ch_name));
			PostCallback(THISBACK(RefreshGui));
		}
		else if (key == "name") {
			Vector<String> args = Split(message, " ");
			if (args.GetCount() != 2) throw Exc("Polling argument error");
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
	for(int i = 0; i < channels.GetCount(); i++) ch_rem.Add(channels.GetKey(i));
	
	int ch_count = in.Get32();
	bool fail = false;
	for(int i = 0; i < ch_count; i++) {
		int name_len = in.Get32();
		if (name_len <= 0) continue;
		String name = in.Get(name_len);
		
		if (ch_rem.Find(name) != -1) ch_rem.RemoveKey(name);
		
		Channel& ch = channels.GetAdd(name);
	}
	if (fail) throw Exc("Getting channellist failed");
	
	for(int i = 0; i < ch_rem.GetCount(); i++)
		channels.RemoveKey(ch_rem[i]);
	
	Print("Client " + IntStr(user_id) + " updated channellist (size " + IntStr(ch_count) + ")");
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
		
		User& u = users.GetAdd(user_id);
		u.user_id = user_id;
		u.name = name;
		if (u.name.GetCount() != name_len) fail = true;
		
		int channel_count = in.Get32();
		if (channel_count < 0 || channel_count >= 200) {fail = true; continue;}
		for(int j = 0; j < channel_count; j++) {
			int ch_len = in.Get32();
			String ch_name = in.Get(ch_len);
			u.channels.FindAdd(ch_name);
			Channel& ch = channels.GetAdd(ch_name);
			ch.userlist.FindAdd(user_id);
		}
	}
	if (fail) throw Exc("Getting userlist failed");
	
	Print("Client " + IntStr(user_id) + " updated userlist (size " + IntStr(user_count) + ")");
}

void Client::RefreshGui() {
	ChannelList& cl = irc.GetChannelList();
	cl.SetChannelCount(channels.GetCount());
	
	for(int i = 0; i < channels.GetCount(); i++) {
		cl.SetChannelName(i, channels.GetKey(i));
		cl.SetChannelUnread(i, channels[i].unread);
	}
	
	String active_channel = irc.GetActiveChannel();
	if (!active_channel.IsEmpty()) {
		Channel& ch = channels.GetAdd(active_channel);
		
		for(int i = ch.messages.GetCount() - ch.unread; i < ch.messages.GetCount(); i++) {
			const ChannelMessage& msg = ch.messages[i];
			irc.Post(msg);
		}
		ch.unread = 0;
		
		irc.SetUserCount(ch.userlist.GetCount());
		for(int i = 0; i < ch.userlist.GetCount(); i++) {
			User& u = users.GetAdd(ch.userlist[i]);
			irc.SetUser(i, u.name);
		}
	}
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
		irc.SetUser(i, u.name);
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
			String channel = args[1];
			Join(channel);
			irc.SetActiveChannel(channel);
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
			if (!Set("name", name)) {
				user_name = name;
				RefreshGui();
			}
		}
	}
	else {
		String active_channel = irc.GetActiveChannel();
		Channel& ch = channels.GetAdd(active_channel);
		ch.Post(user_id, user_name, cmd);
		RefreshGui();
		SendChannelMessage(active_channel, cmd);
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
	
	users.AddColumn("Name");
	
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

void IrcCtrl::SetUser(int i, String s) {
	users.Set(i, 0, s);
}

