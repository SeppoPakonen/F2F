#include "Server.h"


UserDatabase::UserDatabase() {
	Value(200); //NICK
	Value(sizeof(int64)); //PASSHASH
	Value(sizeof(Time)); //JOINED
	Value(sizeof(Time)); //LASTLOGIN
	Value(sizeof(int64)); //LOGINS
	Value(sizeof(int64)); //ONLINETOTAL
	Value(sizeof(int64)); //VISIBLETOTAL
	Value(sizeof(double)); //LL_LON
	Value(sizeof(double)); //LL_LAT
	Value(sizeof(double)); //LL_ELEVATION
	Value(sizeof(Time)); //LL_UPDATED
	Value(0); //CHANNEL_BEGIN
}

int UserDatabase::Init(int user_id) {
	this->user_id = user_id;
	
	String user_dir = ConfigFile("users");
	RealizeDirectory(user_dir);
	
	String user_file = AppendFileName(user_dir, IntStr(user_id) + ".bin");
	String user_loc_file = AppendFileName(user_dir, IntStr(user_id) + "_loc.bin");
	
	if (!user.Open(user_file))
		return 1;
	
	if (!location.Open(user_loc_file))
		return 1;
	
	open = true;
	
	return 0;
}

void UserDatabase::Deinit() {
	user.Close();
	location.Close();
}

void UserDatabase::SetTime(int key, Time value) {
	switch (key) {
		case JOINED:
		case LASTLOGIN:
		case LL_UPDATED: break;
		default: Panic("ERROR");
	}
	int offset = offsets[key];
	user.Seek(offset);
	user.Put(&value, sizeof(Time));
}

void UserDatabase::SetStr(int key, String value) {
	switch (key) {
		case NICK: break;
		default: Panic("ERROR");
	}
	if (value.GetCount() > 200) value = value.Left(200);
	int offset = offsets[key];
	user.Seek(offset);
	user.Put(value.Begin(), value.GetCount());
	if (value.GetCount() < 200) user.Put("", 1);
}

void UserDatabase::SetInt(int key, int64 value) {
	switch (key) {
		case PASSHASH:
		case LOGINS:
		case ONLINETOTAL:
		case VISIBLETOTAL: break;
		default: Panic("ERROR");
	}
	int offset = offsets[key];
	user.Seek(offset);
	user.Put(&value, sizeof(int64));
}

void UserDatabase::SetDbl(int key, double value) {
	switch (key) {
		case LL_LON:
		case LL_LAT:
		case LL_ELEVATION: break;
		default: Panic("ERROR");
	}
	int offset = offsets[key];
	user.Seek(offset);
	user.Put(&value, sizeof(double));
}

void UserDatabase::Flush() {
	user.Flush();
}

int64 UserDatabase::GetInt(int key) {
	switch (key) {
		case PASSHASH:
		case LOGINS:
		case ONLINETOTAL:
		case VISIBLETOTAL: break;
		default: Panic("ERROR");
	}
	int offset = offsets[key];
	int64 value;
	user.Seek(offset);
	user.Get(&value, sizeof(int64));
	return value;
}

String UserDatabase::GetString(int key) {
	switch (key) {
		case NICK: break;
		default: Panic("ERROR");
	}
	int offset = offsets[key];
	String value;
	user.Seek(offset);
	for(int i = 0; i < 200; i++) {
		char c;
		user.Get(&c, 1);
		if (c == 0) break;
		value.Cat(c);
	}
	return value;
}

Time UserDatabase::GetTime(int key) {
	switch (key) {
		case JOINED:
		case LASTLOGIN:
		case LL_UPDATED: break;
		default: Panic("ERROR");
	}
	int offset = offsets[key];
	Time value;
	user.Seek(offset);
	user.Get(&value, sizeof(Time));
	return value;
}

double UserDatabase::GetDbl(int key) {
	switch (key) {
		case LL_LON:
		case LL_LAT:
		case LL_ELEVATION: break;
		default: Panic("ERROR");
	}
	int offset = offsets[key];
	double value;
	user.Seek(offset);
	user.Get(&value, sizeof(double));
	return value;
}

int UserDatabase::AddChannel(const String& channel) {
	ASSERT(channel.GetCount() <= 200);
	int offset = offsets[CHANNEL_BEGIN];
	user.Seek(offset);
	int i = 0;
	while (!user.IsEof()) {
		char c;
		user.Get(&c, 1);
		if (!c) {
			// fill empty record
			user.SeekCur(-1);
			user.Put(channel.Begin(), channel.GetCount());
			for(int i = channel.GetCount(); i < 200; i++) user.Put("", 1);
			Time now = GetUtcTime();
			user.Put(&now, sizeof(Time));
			return i;
		}
		user.SeekCur(channel_record-1);
		i++;
	}
	user.Put(channel.Begin(), channel.GetCount());
	for(int i = channel.GetCount(); i < 200; i++) user.Put("", 1);
	Time now = GetUtcTime();
	user.Put(&now, sizeof(Time));
	return i;
}

int UserDatabase::GetChannelCount() {
	int64 size = user.GetSize();
	return (size - offsets[CHANNEL_BEGIN]) / channel_record;
}

String UserDatabase::GetChannel(int i) {
	int offset = offsets[CHANNEL_BEGIN];
	offset += i * channel_record;
	user.Seek(offset);
	String ch_name;
	for(int i = 0; i < 200; i++) {
		char c;
		user.Get(&c, 1);
		if (!c) break;
		ch_name.Cat(c);
	}
	return ch_name;
}

int UserDatabase::FindChannel(const String& channel) {
	int ch_count = GetChannelCount();
	for(int i = 0; i < ch_count; i++)
		if (GetChannel(i) == channel)
			return i;
	return -1;
}

void UserDatabase::DeleteChannel(int i) {
	int offset = offsets[CHANNEL_BEGIN];
	offset += i * channel_record;
	user.Seek(offset);
	user.Put("", 1);
}

void UserDatabase::SetLocation(double longitude, double latitude, double elevation) {
	Time now = GetUtcTime();
	
	SetDbl(LL_LON, longitude);
	SetDbl(LL_LAT, latitude);
	SetDbl(LL_ELEVATION, elevation);
	SetTime(LL_UPDATED, now);
	
	location.Put(&longitude, sizeof(double));
	location.Put(&latitude, sizeof(double));
	location.Put(&elevation, sizeof(double));
	location.Put(&now, sizeof(Time));
	location.Flush();
}





ServerDatabase::ServerDatabase() {
	
}

void ServerDatabase::Init() {
	String srv_file = ConfigFile("server.bin");
	if (!file.Open(srv_file))
		throw Exc("Couldn't open server file");
	
	int64 size = file.GetSize();
	user_count = size / user_record;
}

int ServerDatabase::GetUserCount() {
	return user_count;
}

void ServerDatabase::AddUser(String user) {
	ASSERT(user.GetCount() <= 200);
	file.SeekEnd();
	file.Put(user.Begin(), user.GetCount());
	for(int i = user.GetCount(); i < 200; i++)
		file.Put("", 1);
	user_count++;
}

int ServerDatabase::FindUser(const String& user) {
	int user_count = GetUserCount();
	for(int i = 0; i < user_count; i++) {
		String u = GetUser(i);
		if (u == user)
			return i;
	}
	return -1;
}

String ServerDatabase::GetUser(int i) {
	int offset = i * user_record;
	file.Seek(offset);
	String value;
	for(int i = 0; i < 200; i++) {
		char c;
		file.Get(&c, 1);
		if (!c) break;
		value.Cat(c);
	}
	return value;
}

void ServerDatabase::SetUser(int user_id, String value) {
	ASSERT(value.GetCount() <= 200);
	int offset = user_id * user_record;
	file.Seek(offset);
	file.Put(value.Begin(), value.GetCount());
	for(int i = value.GetCount(); i < 200; i++)
		file.Put("", 1);
}

void ServerDatabase::Flush() {
	file.Flush();
}


