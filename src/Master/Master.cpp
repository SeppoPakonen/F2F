#include "Master.h"

void Print(const String& s) {
	static SpinLock lock;
	lock.Enter();
	Cout() << s;
	Cout().PutEol();
	lock.Leave();
}



Master::Master() {
	LoadThis();
}

void Master::Run() {
	if(!listener.Listen(17123, 5)) {
		Print("Unable to initialize server socket!");
		SetExitCode(1);
		return;
	}
	Print("Waiting for requests..");
	while (!Thread::IsShutdownThreads()) {
		One<TcpSocket> ses;
		ses.Create();
		if(ses->Accept(listener)) {
			ses->Timeout(30000);
			Thread::Start(THISBACK1(Session, ses.Detach()));
		}
	}
}

void Master::Session(One<TcpSocket> t) {
	try {
		String addr;
		uint16 port;
		int i, r, addr_len;
		
		r = t->Get(&addr_len, sizeof(uint32));
		if (r != sizeof(uint32)) throw Exc("Reading remote address failed");
		if (addr_len < 0 || addr_len >= 200) throw Exc("Invalid address");
		
		// Asks only server list
		if (addr_len == 0) {
			lock.EnterRead();
			i = servers.GetCount();
			t->Put(&i, sizeof(int));
			for(int i = 0; i < servers.GetCount(); i++) {
				const Server& s = servers[i];
				i = s.addr.GetCount();
				t->Put(&i, sizeof(int));
				t->Put(s.addr.Begin(), s.addr.GetCount());
				t->Put(&s.port, sizeof(uint16));
			}
			lock.LeaveRead();
		}
		
		// Registers a server
		else {
			addr = t->Get(addr_len);
			if (addr.GetCount() != addr_len) throw Exc("Invalid address");
			
			r = t->Get(&port, sizeof(uint16));
			if (r != sizeof(uint16)) throw Exc("Invalid port");
			
			TcpSocket test;
			if (test.Connect(addr, port)) {
				lock.EnterWrite();
				Server& s = servers.Add();
				s.addr = addr;
				s.port = port;
				StoreThis();
				lock.LeaveWrite();
			}
		}
	}
	catch (Exc e) {
		Print("Unexpected error from " + t->GetPeerAddr() + ": " + e);
	}
	catch (...) {
		Print("Unexpected error from " + t->GetPeerAddr());
	}
	t->Close();
}

CONSOLE_APP_MAIN
{
	Master().Run();
}
