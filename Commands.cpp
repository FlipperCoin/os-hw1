#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <limits.h>
#include <algorithm>
#include "Commands.h"

using namespace std;

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundCommand(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

void _serror(string error) {
  cerr << ("smash error: " + error).c_str() << endl;
}

void _serrorSys(string sysname) {
  perror(("smash error: " + sysname + " failed").c_str());
}

char* _getCwd() {
  int size = PATH_MAX+1;
  char* buf = new char[size];
  if (!getcwd(buf,size)) {
    _serrorSys("getcwd");
    return nullptr;
  }
  return buf;
}

// TODO: Add your implementation for classes in Commands.h 
Command::Command(const char* cmd_line) {
  this->cmd_line = string(cmd_line);

  char** args = new char*[COMMAND_MAX_ARGS];
  for (int i = 0; i < COMMAND_MAX_ARGS; i++) {
    args[i] = new char[COMMAND_ARGS_MAX_LENGTH];
  }
  
  int argc = _parseCommandLine(cmd_line,args);
  
  for (int i = 0; i < argc; i++) {
    this->args.push_back(string(args[i]));
    delete args[i];
  }
  delete[] args;
}

Command::~Command() {}

void GetCurrDirCommand::execute() {
  char* cwd = _getCwd();
  if (!cwd) {
    return;
  }
  cout << cwd << endl;
  delete[] cwd;
}

ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};

void ShowPidCommand::execute()
{
  cout << "smash pid is " << getpid() << "\n";
}

ChPromptCommand::ChPromptCommand(const char* cmd_line) : BuiltInCommand(cmd_line)
{
  this->prompt_name = this->args.size() > 1 ? this->args[1] : DEFAULT_PROMPT;
}

void ChPromptCommand::execute()
{
  SmallShell& smash = SmallShell::getInstance();
  smash.setName(this->prompt_name);
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, string* plast_pwd) : BuiltInCommand(cmd_line) {
  this->plast_pwd = plast_pwd;
}

void ChangeDirCommand::execute() {
  if (args.size() <= 1) {
    return;
  }

  if (args.size() > 2) {
    _serror("cd: too many arguments");
    return;
  }

  string new_wd = args[1];

  if (args[1] == "-") {
      if (plast_pwd->empty()) {
        _serror("cd: OLDPWD not set");
        return;
      }
      
      new_wd = *plast_pwd;
  }

  char *new_last_pwd= _getCwd();

  if (-1 == chdir(new_wd.c_str())) {
    _serrorSys("chdir");
    return;
  }

  // TODO: not sure what to do on failure, prob undefined behaviour by segel..
  if (new_last_pwd) {
    *plast_pwd = string(new_last_pwd);
    delete[] new_last_pwd;
  } else {
    *plast_pwd = "";
  }
}

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line) {
  this->jobs = jobs;
}

void JobsCommand::execute() {
  jobs->printJobsList();
}

ExternalCommand::ExternalCommand(const char* cmd_line, JobsList* jobs) : Command(cmd_line) {
  this->jobs = jobs;
  this->isBackgroundCommand = _isBackgroundCommand(cmd_line);
}

char* ExternalCommand::createCmdStr() {
  char *cmd_line_noamp = new char[cmd_line.length()+1];
  strcpy(cmd_line_noamp, cmd_line.c_str());
  _removeBackgroundSign(cmd_line_noamp);
  return cmd_line_noamp;
}

void ExternalCommand::execute() {
  pid_t pid = fork();
  if (-1 == pid) {
    _serrorSys("fork");
    return;
  }

  if (pid == 0) {
    if (-1 == setpgrp()) {
      _serrorSys("setgrp");
      exit(1);
    }
    char *cmd_str = createCmdStr();
    char bash_path[10];
    strcpy(bash_path,"/bin/bash");
    char bash_flag[3];
    strcpy(bash_flag,"-c");
    char* argv[4] = {bash_path,bash_flag,cmd_str,nullptr};
    execv(bash_path,argv);
    _serrorSys("execv");
    delete cmd_str;
  }
  
  int wstatus;
  if (this->isBackgroundCommand) {
    this->jobs->addJob(this, pid);
  } else {
    int err = waitpid(pid, &wstatus, WUNTRACED);
    if (-1 == err) {
      _serrorSys("waitpid");
    }
    // TODO: should print "smash: process <foreground-PID> was stopped"?
    // or should it be in shell's SIGSTP handler (after sending the stop signal)?
    // if (WIFSTOPPED(wstatus)) {
    //   ...
    // }
  }
}

void SmallShell::setName(string prompt_name)
{
  this->prompt_name = prompt_name;
}

string SmallShell::getName()
{
  return this->prompt_name;
}

ShowPidCommand::~ShowPidCommand() {};
ChPromptCommand::~ChPromptCommand() {};

SmallShell::SmallShell() : prompt_name(DEFAULT_PROMPT), plast_pwd(new string()) {
// TODO: add your implementation
}

SmallShell::~SmallShell() {
// TODO: add your implementation
  delete plast_pwd;
}

// TODO: why is there a space/tab in the last job in every example in the PDF
// TODO: make sure jid is sorted!
void JobsList::printJobEntry(JobEntry& job) {
  cout << "[" << job.jid << "] ";
  // TODO: should we print the orig cmd as it was typed, or is this reconstruction fine
  for (string& arg : job.args) {
    cout << arg << " ";
  }
  cout << ": ";
  time_t now = time(nullptr);
  if ((time_t)-1 == now) {
    _serrorSys("time");
    // TODO: stop? print something else?
  }
  cout << job.pid << " " << difftime(now, job.start_time);
  if (job.is_stopped) {
    cout << " (stopped)";
  }
  cout << endl;
}

void JobsList::printJobsList() {
  clearZombieJobs();
  for (JobEntry& job : jobs) {
    printJobEntry(job);
  }
}

void JobsList::addJob(Command* cmd, pid_t pid, bool isStopped) {
  clearZombieJobs();

  int max_jid = !jobs.empty() ? jobs.end()->jid : 0;
  int jid = max_jid+1;

  time_t now = time(nullptr);
  if ((time_t)-1 == now) {
    _serrorSys("time");
    // TODO: ignore? fail?
  }

  JobEntry job(jid,pid,cmd->args,cmd->cmd_line,now,isStopped);
  jobs.push_back(job);
}

void JobsList::clearZombieJobs() {
  auto end = remove_if(
    jobs.begin(),
    jobs.end(),
    [](JobEntry const& job) {
      int wstatus;
      waitpid(job.pid,&wstatus,WNOHANG);
      return (WIFEXITED(wstatus) || WIFSIGNALED(wstatus));
    });

  jobs.erase(end,jobs.end());
}

JobsList::JobEntry::JobEntry(jobid_t jid, pid_t pid, vector<string> args, string cmd_line, time_t start_time, bool is_stopped)
  : jid(jid), pid(pid), args(args), cmd_line(cmd_line), start_time(start_time), is_stopped(is_stopped) {} 

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

  if (firstWord.compare("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line);
  }
  else if (firstWord.compare("showpid") == 0) {
    return new ShowPidCommand(cmd_line);
  }
  else if(firstWord.compare("chprompt") == 0)
  { 
    return new ChPromptCommand(cmd_line);
  }
  else if (firstWord.compare("cd") == 0) {
    return new ChangeDirCommand(cmd_line, this->plast_pwd);
  }
  else if (firstWord.compare("jobs") == 0) {
    return new JobsCommand(cmd_line, &(this->jobs));
  }
  else {
    return new ExternalCommand(cmd_line, &(this->jobs));
  }
  
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: Add your implementation here
  // for example:
    Command* cmd = CreateCommand(cmd_line);
    if (cmd) {
      cmd->execute();
      delete cmd;
    }
  //Please note that you must fork smash process for some commands (e.g., external commands....)
}