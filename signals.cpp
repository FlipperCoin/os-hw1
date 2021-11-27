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
    smash.jobs.addJob(smash.fg_cmd, smash.fg_pid, true);
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

void alarmHandler(int sig_num, siginfo_t* info, void *ucontext) {
  SmallShell& smash = SmallShell::getInstance();
  cout << "smash got an alarm" << endl;
  
  pid_t pid = info->si_pid;
  string cmd_line;
  auto job = smash.jobs.getJobByPid(pid);
  if (nullptr != job) {
    cmd_line = job->cmd_line;
    smash.jobs.removeJobById(job->jid);
  }
  else if (smash.fg_pid == pid) {
    cmd_line = smash.fg_cmd;
  }
  else {
    return;
  }

  cout << "smash: " << cmd_line << " timed out!" << endl;
  if (0 != kill(pid, SIGKILL)) {
    _serrorSys("kill");
  }
}

