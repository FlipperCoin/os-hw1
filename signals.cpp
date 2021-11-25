#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>

using namespace std;

void ctrlZHandler(int sig_num) {
	// TODO: Add your implementation
}

void ctrlCHandler(int sig_num) {
  SmallShell& smash = SmallShell::getInstance();
  if(smash.fg_pid != getpid())
  {
    cout << "smash: got ctrl-C" << endl;
    cout << "smash: process " << smash.fg_pid << " was killed" << endl;
    if(kill(smash.fg_pid,SIGKILL) == -1)
    {
      _serrorSys("kill");
    }
  }
}

void alarmHandler(int sig_num) {
  // TODO: Add your implementation
}

