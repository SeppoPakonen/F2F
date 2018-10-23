#include "Server.h"

namespace Config {

INI_STRING(server_title, "Unnamed server", "Server name");
INI_INT(port, 17000, "Server port");
INI_INT(max_sessions, 10000, "Max clients");
INI_INT(max_image_size, 100000, "Max profile image size in bytes");
INI_INT(max_set_string_len, 1000000, "Max set function string size in bytes");
INI_STRING(master_addr, "93.170.105.68", "Master server's address");
INI_INT(master_port, 17123, "Master server's port");

};

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
	Title("F2F Server");
	MinimizeBox().MaximizeBox().Sizeable();
	
	LoadThis();
	
	AddFrame(menubar);
	menubar.Set(THISBACK(MainMenu));
	
	usermode.Add("Active sessions");
	usermode.Add("All users");
	usermode.SetIndex(0);
	userlist.AddColumn("User-id");
	userlist.AddColumn("Name");
	userlist <<= THISBACK(Data);
	userctrl.Add(usermode.TopPos(0, 30).HSizePos());
	userctrl.Add(userlist.VSizePos(30).HSizePos());
	usersesslist.AddColumn("Begin");
	usersesslist.AddColumn("End");
	usersesslist <<= THISBACK(Data);
	userchannels.AddColumn("Channel");
	userdetails.AddColumn("Key");
	userdetails.AddColumn("Value");
	usermainsplit.Horz();
	usermainsplit << userchannels << userdetails;
	usermainsplit.SetPos(3333);
	userlog.AddColumn("Time");
	userlog.AddColumn("Message");
	userlog.ColumnWidths("1 4");
	usertabs.Add(usermainsplit.SizePos(), "Main");
	usertabs.Add(userlog.SizePos(), "Log");
	split << userctrl << usersesslist << usertabs;
	split.SetPos(2500, 0);
	split.SetPos(5000, 1);
	split.Horz();
	tabs.Add(split.SizePos(), "Users");
	
	serverlog.AddColumn("Time");
	serverlog.AddColumn("Message");
	serverlog.ColumnWidths("1 6");
	tabs.Add(serverlog.SizePos(), "Server log");
	
	analyzectrl.Add(analyze_chlist.VSizePos().LeftPos(0, 150));
	analyzectrl.Add(analyze_mapctrl.VSizePos().HSizePos(150));
	analyze_chlist.AddColumn("Channel");
	analyze_chlist <<= THISBACK(ChangeLocation);
	analyze_mapctrl.Set(Pointf(25.46748, 65.05919));
	analyze_mapctrl.WhenMove << THISBACK(ChangeLocation);
	tabs.Add(analyzectrl.SizePos(), "Analyze");
	
	Add(tabs.SizePos());
	tabs.WhenSet << THISBACK(Data);
	
	tc.Set(1000, THISBACK(TimedRefresh));
}

Server::~Server() {
	RemoveBots();
	running = false; listener.Close(); while (!stopped) Sleep(100);
	StoreThis();
	db.Flush();
}

void Server::TimedRefresh() {
	PostCallback(THISBACK(Data));
	tc.Set(1000, THISBACK(TimedRefresh));
}

void Server::MainMenu(Bar& bar) {
	bar.Sub("Server", [=](Bar& bar) {
		bar.Add("Add bots", THISBACK(AddBots));
		bar.Add("Remove bots", THISBACK(RemoveBots));
		bar.Separator();
		bar.Add("Close session connection", THISBACK(CloseSession)).Key(K_CTRL|K_C);
		bar.Separator();
		bar.Add("Analyze", THISBACK(ChangeLocation)).Key(K_CTRL|K_A);
	});
}

void Server::AddBots() {
	for(int i = 0; i < 10; i++) {
		Client& c = clients.Add();
		c.SetId(clients.GetCount() - 1);
		c.Start();
	}
}

void Server::RemoveBots() {
	for(int i = 0; i < clients.GetCount(); i++) {
		Client& c = clients[i];
		c.Stop();
	}
	for(int i = 0; i < clients.GetCount(); i++)
		clients[i].Wait();
}

void Server::CloseSession() {
	int user_cursor = userlist.GetCursor();
	if (user_cursor >= 0 && user_cursor < userlist.GetCount()) {
		int user_id = userlist.Get(user_cursor, 0);
		const UserDatabase& db = GetDatabase(user_id);
		int usersess_cursor = usersesslist.GetCursor();
		if (usersess_cursor >= 0 && usersess_cursor < db.sessions.GetCount()) {
			int sess_id = db.sessions.GetKey(usersess_cursor);
			//const UserSessionLog& usl = db.sessions[usersess_cursor];
			int i = sessions.Find(sess_id);
			if (i != -1) {
				sessions[i].Stop();
			}
		}
	}
}

void Server::Print(const String& s) {
	LOG(s);
	log.Add().Set(s);
	if (tabs.Get() == SRVLOG_TAB)
		PostCallback(THISBACK(Data));
}

void Server::Init() {
	db.Init();
}

void Server::Data() {
	int tab = tabs.Get();
	
	if (tab == USER_TAB) {
		int mode = usermode.GetIndex();
		if (mode == 0) {
			int row = 0;
			for(int i = 0; i < sessions.GetCount(); i++) {
				const ActiveSession& s = sessions[i];
				if (s.last_user_id == -1) continue;
				UserDatabase& db = GetDatabase(s.last_user_id);
				userlist.Set(row, 0, s.last_user_id);
				userlist.Set(row, 1, db.name);
				row++;
			}
			userlist.SetCount(row);
		}
		else if (mode == 1) {
			int row = 0;
			for(int i = 0; i < db.GetUserCount(); i++) {
				userlist.Set(row, 0, i);
				userlist.Set(row, 1, db.GetUser(i));
				row++;
			}
			userlist.SetCount(row);
		}
		
		int user_cursor = userlist.GetCursor();
		if (user_cursor >= 0 && user_cursor < userlist.GetCount()) {
			int user_id = userlist.Get(user_cursor, 0);
			
			const UserDatabase& db = GetDatabase(user_id);
			
			for(int i = 0; i < db.sessions.GetCount(); i++) {
				const UserSessionLog& usl = db.sessions[i];
				usersesslist.Set(i, 0, usl.begin);
				usersesslist.Set(i, 1, usl.begin < usl.end ? Format("%", usl.end) : "");
			}
			usersesslist.SetCount(db.sessions.GetCount());
			
			int usersess_cursor = usersesslist.GetCursor();
			if (usersess_cursor >= 0 && usersess_cursor < db.sessions.GetCount()) {
				const UserSessionLog& usl = db.sessions[usersess_cursor];
				int usertab = usertabs.Get();
				
				if (usertab == 0) {
					for(int i = 0; i < db.channels.GetCount(); i++) {
						userchannels.Set(i, 0, db.channels[i]);
					}
					userchannels.SetCount(db.channels.GetCount());
					int row = 0;
					userdetails.Set(row, 0, "User id");
					userdetails.Set(row++, 1, user_id);
					userdetails.Set(row, 0, "Name");
					userdetails.Set(row++, 1, db.name);
					userdetails.Set(row, 0, "Profile image");
					userdetails.Set(row, 1, StreamRaster::LoadStringAny(db.profile_img));
					if (db.profile_img.IsEmpty())
						userdetails.SetDisplay(row++, 1, StdDisplay());
					else
						userdetails.SetDisplay(row++, 1, ImageDisplay());
					userdetails.Set(row, 0, "Age");
					userdetails.Set(row++, 1, (int)db.age);
					userdetails.Set(row, 0, "Gender");
					userdetails.Set(row++, 1, db.gender ? "Male" : "Female");
					userdetails.Set(row, 0, "Profile image hash");
					userdetails.Set(row++, 1, (int64)db.profile_img_hash);
					userdetails.Set(row, 0, "Password hash");
					userdetails.Set(row++, 1, (int64)db.passhash);
					userdetails.Set(row, 0, "Joined");
					userdetails.Set(row++, 1, db.joined);
					userdetails.Set(row, 0, "Last login");
					userdetails.Set(row++, 1, db.lastlogin);
					userdetails.Set(row, 0, "Logins");
					userdetails.Set(row++, 1, db.logins);
					userdetails.Set(row, 0, "Online-total");
					userdetails.Set(row++, 1, db.onlinetotal);
					userdetails.Set(row, 0, "Visible-total");
					userdetails.Set(row++, 1, db.visibletotal);
					userdetails.Set(row, 0, "Latitude");
					userdetails.Set(row++, 1, db.latitude);
					userdetails.Set(row, 0, "Longitude");
					userdetails.Set(row++, 1, db.longitude);
					userdetails.Set(row, 0, "Elevation");
					userdetails.Set(row++, 1, db.elevation);
					userdetails.Set(row, 0, "Last update");
					userdetails.Set(row++, 1, db.lastupdate);
					userdetails.SetCount(row);
				}
				else if (usertab == 1) {
					for(int i = 0; i < usl.log.GetCount(); i++) {
						const LogItem& li = usl.log[i];
						userlog.Set(i, 0, li.added);
						userlog.Set(i, 1, li.msg);
					}
					userlog.SetCount(usl.log.GetCount());
				}
			}
		}
	}
	else if (tab == SRVLOG_TAB) {
		for(int i = serverlog.GetCount(); i < log.GetCount(); i++) {
			const LogItem& l = log[i];
			serverlog.Set(i, 0, l.added);
			serverlog.Set(i, 1, l.msg);
		}
		serverlog.ScrollEnd();
	}
	else if (tab == ANAL_TAB) {
		for(int i = 0; i < db.channels.GetCount(); i++) {
			analyze_chlist.Set(i, 0, db.channels[i].name);
		}
		
	}
	
}

void Server::Listen() {
	while (!Thread::IsShutdownThreads() && running) {
		try {
			if(!listener.Listen(Config::port, 5)) {
				throw Exc("Unable to initialize server socket!");
				SetExitCode(1);
				return;
			}
			
			Print("Registering server to the master server list");
			TcpSocket master;
			master.Timeout(10000);
			if (!master.Connect((String)Config::master_addr, Config::master_port))
				throw Exc("Unable to connect the master server list");
			#ifdef flagPOSIX
			Sleep(1000); // weird bug occurs in linux without this
			#endif
			uint16 port = Config::port;
			int r = master.Put(&port, sizeof(uint16));
			if (r != sizeof(uint16)) throw Exc("Master server connection failed (1)");
			
			bool success = false;
			for(int i = 0; i < 1000; i++) {
				TcpSocket verify;
				verify.Timeout(10000);
				if (!verify.Accept(listener)) continue;
				int chk;
				r = verify.Get(&chk, sizeof(int));
				if (r != sizeof(int) || chk != 12345678) continue;
				r = verify.Put(&chk, sizeof(int));
				if (r != sizeof(int)) continue;
				
				int ret;
				r = master.Get(&ret, sizeof(int));
				if (r != sizeof(int)) continue;
				if (ret != 0) continue;
				master.Close();
				
				success = true;
				break;
			}
			if (!success) throw Exc("Master server couldn't connect this server");
			
			Print("Waiting for requests..");
			TimeStop ts;
			while (listener.IsOpen() && !Thread::IsShutdownThreads() && running) {
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
						
						// Normal session
						if (sessions.GetCount() < Config::max_sessions) {
							lock.EnterWrite();
							sessions.Add(id, ses.Detach()).Start();
							lock.LeaveWrite();
							
							if (tabs.Get() == USER_TAB)
								PostCallback(THISBACK(Data));
						}
					}
				}
				
				if (ts.Elapsed() > 1000) {
					lock.EnterWrite();
					for(int i = 0; i < sessions.GetCount(); i++) {
						if (!sessions[i].s.IsOpen()) {
							sessions.Remove(i);
							i--;
						}
					}
					lock.LeaveWrite();
					ts.Reset();
					
					if (tabs.Get() == USER_TAB)
						PostCallback(THISBACK(Data));
				}
			}
		}
		catch (Exc e) {
			Print(e);
			Sleep(1000);
		}
	}
	
	for(int i = 0; i < sessions.GetCount(); i++)
		sessions[i].s.Close();
	
	for(int i = 0; i < sessions.GetCount(); i++)
		while (!sessions[i].stopped)
			Sleep(100);
	
	stopped = true;
}

void Server::SendToAll(int user_id, String msg) {
	lock.EnterWrite();
	Index<int> users;
	GetUserlist(users, user_id);
	SendMessage(user_id, msg, users);
	lock.LeaveWrite();
}

void Server::GetUserlist(Index<int>& userlist, int user_id) {
	UserDatabase& db = GetDatabase(user_id);
	for(int i = 0; i < db.channels.GetCount(); i++) {
		int j = db.channels[i];
		if (j >= 0 && j < this->db.channels.GetCount()) {
			const Channel& ch = this->db.channels[j];
			for(int j = 0; j < ch.users.GetCount(); j++)
				userlist.FindAdd(ch.users[j]);
		}
	}
}

int64 Server::GetNewLoginId() {
	while (true) {
		int64 login_id = Random(INT_MAX) | (Random(INT_MAX) << 32);
		if (login_session_ids.Find(login_id) == -1)
			return login_id;
	}
}

void Server::JoinChannel(const String& channel, int user_id) {
	if (channel.IsEmpty()) return;
	lock.EnterWrite();
	int id = db.channels.Find(channel);
	if (id == -1) {
		id = db.channels.GetCount();
		Channel& ch = db.channels.Add(channel);
		ch.name = channel;
	}
	Channel& ch = db.channels.Get(channel);
	SendMessage(user_id, "join " + IntStr(user_id) + " " + channel, ch.users);
	ch.users.FindAdd(user_id);
	UserDatabase& user_db = GetDatabase(user_id);
	user_db.channels.Add(id);
	user_db.Flush();
	db.Flush();
	lock.LeaveWrite();
}

void Server::LeaveChannel(const String& channel, int user_id) {
	lock.EnterWrite();
	int id = db.channels.Find(channel);
	if (id != -1) {
		UserDatabase& user_db = GetDatabase(user_id);
		Channel& ch = db.channels[id];
		ch.users.RemoveKey(user_id);
		user_db.channels.RemoveKey(id);
		SendMessage(user_id, "leave " + IntStr(user_id) + " " + channel, ch.users);
		db.Flush();
	}
	lock.LeaveWrite();
}

void Server::SendMessage(int sender_id, const String& msg, const Index<int>& user_list) {
	ASSERT(sender_id >= 0);
	if (msg.IsEmpty()) return;
	const MessageRef& ref = IncReference(msg, user_list.GetCount());
	for(int i = 0; i < user_list.GetCount(); i++) {
		int user_id = user_list[i];
		if (user_id == sender_id) continue;
		UserDatabase& user_db = GetDatabase(user_id);
		user_db.lock.Enter();
		InboxMessage& m = user_db.inbox.Add();
		m.sender_id = sender_id;
		m.msg = ref.hash;
		user_db.lock.Leave();
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

#ifdef flagDEBUG
VectorMap<unsigned, String> message_history;
#endif

MessageRef& Server::GetReference(unsigned hash) {
	msglock.EnterRead();
	int i = messages.Find(hash);
	if (i == -1) {
		msglock.LeaveRead();
		#ifdef flagDEBUG
		i = message_history.Find(hash);
		if (i != -1)
			throw Exc("Message removed: " + message_history[i]);
		else
			throw Exc("Message removed. NEVER EXISTED");
		#endif
		throw Exc("Message removed");
	}
	MessageRef& ref = messages[i];
	msglock.LeaveRead();
	return ref;
}

void Server::DecReference(MessageRef& ref) {
	ref.refcount--;
	if (ref.refcount == 0) {
		msglock.EnterWrite();
		// check again after acquiring the lock
		if (ref.refcount == 0) {
			#ifdef flagDEBUG
			message_history.GetAdd(ref.hash, ref.msg);
			#endif
			messages.RemoveKey(ref.hash);
		}
		msglock.LeaveWrite();
	}
}

bool IsLineIntersection (
	double Ax, double Ay,
	double Bx, double By,
	double Cx, double Cy,
	double Dx, double Dy,
	double &X, double &Y) {
	
	double  distAB, theCos, theSin, newX, ABpos ;
	
	//  Fail if either line is undefined.
	if (Ax==Bx && Ay==By || Cx==Dx && Cy==Dy) return false;
	
	//  (1) Translate the system so that point A is on the origin.
	Bx-=Ax; By-=Ay;
	Cx-=Ax; Cy-=Ay;
	Dx-=Ax; Dy-=Ay;
	
	//  Discover the length of segment A-B.
	distAB=sqrt(Bx*Bx+By*By);
	
	//  (2) Rotate the system so that point B is on the positive X axis.
	theCos=Bx/distAB;
	theSin=By/distAB;
	newX=Cx*theCos+Cy*theSin;
	Cy  =Cy*theCos-Cx*theSin; Cx=newX;
	newX=Dx*theCos+Dy*theSin;
	Dy  =Dy*theCos-Dx*theSin; Dx=newX;
	
	//  Fail if the lines are parallel.
	if (Cy==Dy) return false;
	
	//  (3) Discover the position of the intersection point along line A-B.
	ABpos=Dx+(Cx-Dx)*Dy/(Dy-Cy);
	
	//  (4) Apply the discovered position to line A-B in the original coordinate system.
	X=Ax+ABpos*theCos;
	Y=Ay+ABpos*theSin;
	
	//  Success.
	return true;
}

void Server::ChangeLocation() {
	
	int i = analyze_chlist.GetCursor();
	if (i >= 0 && i < analyze_chlist.GetCount()) {
		String ch = analyze_chlist.Get(i, 0);
		Analyze(ch);
	}
}

void Server::Analyze(String ch) {
	int ch_id = db.channels.Find(ch);
	
	Vector<Vector<Line> > routes;
	
	int user_count = db.GetUserCount();
	for(int i = 0; i < user_count; i++) {
		Vector<Line>& user_routes = routes.Add();
		
		UserDatabase& db0 = GetDatabase(i);
		if (db0.channels.Find(ch_id) == -1) continue;
		
		db0.lock.Enter();
		
		int loc_count = db0.location.GetSize() / (3*sizeof(double) + sizeof(Time));
		db0.location.Seek(0);
		int j = 0;
		double prev_lon, prev_lat;
		while (!db0.location.IsEof()) {
			double lon, lat, elev;
			Time t;
			db0.location.Get(&lon, sizeof(double));
			db0.location.Get(&lat, sizeof(double));
			db0.location.Get(&elev, sizeof(double));
			db0.location.Get(&t, sizeof(Time));
			
			if (j > 0 &&
				prev_lon >= -180 && prev_lon < +180 &&
				lon >= -180 && lon < +180 &&
				prev_lat >= -90 && prev_lat < +90 &&
				lat >= -90 && lat < +90) {
				Line& l = user_routes.Add();
				l.a.x = prev_lon;
				l.a.y = prev_lat;
				l.b.x = lon;
				l.b.y = lat;
			}
			prev_lon = lon;
			prev_lat = lat;
			j++;
		}
		
		db0.lock.Leave();
	}
	
	Vector<Vector<Pointf> > intersections;
	Vector<Pointf> all_intersections;
	for(int i = 0; i < user_count; i++) {
		Vector<Pointf>& user_intersections = intersections.Add();
		
		Vector<Line>& user_routes0 = routes[i];
		
		for(int j = i+1; j < user_count; j++) {
			Vector<Line>& user_routes1 = routes[j];
			
			for (int k0 = 0; k0 < user_routes0.GetCount(); k0++) {
				const Line& l0 = user_routes0[k0];
				for (int k1 = 0; k1 < user_routes1.GetCount(); k1++) {
					const Line& l1 = user_routes1[k1];
					
					Pointf intersection;
					
					if (IsLineIntersection(l0.a.x, l0.a.y, l0.b.x, l0.b.y, l1.a.x, l1.a.y, l1.b.x, l1.b.y, intersection.x, intersection.y)) {
						user_intersections.Add(intersection);
						all_intersections.Add(intersection);
					}
				}
			}
		}
	}
	
	Pointf tl, br;
	tl = GoogleMapsPixelToGps(analyze_mapctrl.center, (int)~analyze_mapctrl.zoom, Size(640,640), Point(0,0));
	br = GoogleMapsPixelToGps(analyze_mapctrl.center, (int)~analyze_mapctrl.zoom, Size(640,640), Point(640,640));
	ImageBuffer ib(640, 640);
	int div = 80;
	Vector<int> sum;
	sum.SetCount(div * div, 0);
	int max = 0;
	for(int i = 0; i < all_intersections.GetCount(); i++) {
		const Pointf& p = all_intersections[i];
		if (p.x > tl.x && p.x < br.x && p.y < tl.y && p.y > br.y) {
			Pointf p2 = p - tl;
			p2.x /= br.x - tl.x;
			p2.y /= br.y - tl.y;
			p2.x *= div;
			p2.y *= div;
			int& j = sum[(int)p2.x + (int)p2.y * div];
			j++;
			if (j > max) max = j;
		}
	}
	
	if (max > 0) {
		double step = 640.0 / div;
		for(int xi = 0; xi < div; xi++) {
			for(int yi = 0; yi < div; yi++) {
				int l = xi * step;
				int r = (xi + 1) * step;
				int t = yi * step;
				int b = (yi + 1) * step;
				int a = sum[xi + yi * div] * 128 / max;
				RGBA c;
				c.r = 255;
				c.g = 1;
				c.b = 1;
				c.a = a;
				DrawRect(ib, Rect(l, t, r, b), c);
			}
		}
	}
	
	analyze_mapctrl.map.overlay = ib;
	analyze_mapctrl.map.Refresh();
}

void DrawRect(ImageBuffer& ib, Rect r, RGBA rgba) {
	RGBA* begin = ib.Begin();
	int w = r.GetWidth();
	int h = r.GetHeight();
	for(int i = 0; i < h; i++) {
		RGBA* cur = begin + (r.top + i) * ib.GetWidth() + r.left;
		for(int j = 0; j < w; j++) {
			*cur = rgba;
			cur++;
		}
	}
}

