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
		uint16 port;
		int i, r;
		
		r = t->Get(&port, sizeof(uint16));
		if (r != sizeof(uint16)) throw Exc("Invalid port");
		
		// Asks only server list
		if (port == 0) {
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
			TcpSocket test;
			i = -1;
			if (test.Connect(t->GetPeerAddr(), port)) {
				int chk1 = 12345678;
				int r = test.Put(&chk1, sizeof(int));
				if (r == sizeof(int)) {
					int chk2 = 0;
					r = test.Get(&chk2, sizeof(int));
					if (r == sizeof(int) && chk2 == chk1) {
						Print("Connected to " + t->GetPeerAddr() + ":" + IntStr(port));
						lock.EnterWrite();
						Server& s = servers.Add();
						s.addr = t->GetPeerAddr();
						s.port = port;
						StoreThis();
						lock.LeaveWrite();
						i = 0;
					}
				}
			}
			if (i == -1) {
				Print("Couldn't connect to " + t->GetPeerAddr() + ":" + IntStr(port));
			}
			t->Put(&i, sizeof(int));
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
