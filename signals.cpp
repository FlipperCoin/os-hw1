#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <unistd.h>

using namespace std;

void ctrlZHandler(int sig_num) {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash: got ctrl-Z" << endl;
  if (smash.fg_pid != getpid()) {
    cout << "smash: process " << smash.fg_pid << " was stopped" << endl;
    if(kill(smash.fg_pid,SIGSTOP) == -1)
    {
      _serrorSys("kill");
    }
    smash.jobs.addJob(smash.fg_cmd, smash.fg_pid,smash.fg_jid, true);
  }
}

void ctrlCHandler(int sig_num) {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash: got ctrl-C" << endl;
  if(smash.fg_pid != getpid())
  {
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

