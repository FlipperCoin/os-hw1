#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include <limits.h>
#include <algorithm>
#include <fcntl.h>
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

ExternalCommand::ExternalCommand(const char* cmd_line, JobsList* jobs,pid_t* fg_pid) : Command(cmd_line), fg_pid(fg_pid) 
{
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
    *fg_pid = pid;
    int err = waitpid(pid, &wstatus, WUNTRACED);
    *fg_pid = getpid();
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
  cout << job.pid << " " << difftime(now, job.start_time) << " secs";
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

  //int max_jid = !jobs.empty() ? jobs.end()->jid : 0; //this doesnt work correctly for some reason!?
  if(!jobs.empty())
  {
    next_jid++; // used inernal max counter.
  }
  else
  {
    next_jid = 1; // reset inernal max counter when list is empty.
  }
  

  time_t now = time(nullptr);
  if ((time_t)-1 == now) {
    _serrorSys("time");
    // TODO: ignore? fail?
  }

  JobEntry job(next_jid,pid,cmd->args,cmd->cmd_line,now,isStopped);
  jobs.push_back(job);
}

void JobsList::clearZombieJobs() {
  auto end = remove_if(
    jobs.begin(),
    jobs.end(),
    [](JobEntry const& job) {
      int wstatus;
      return (0 != waitpid(job.pid,&wstatus,WNOHANG) 
        && (WIFEXITED(wstatus) || WIFSIGNALED(wstatus)));
    });

  jobs.erase(end,jobs.end());
}

JobsList::JobEntry* JobsList::getJobById(jobid_t jobId)
{
  for (int i = 0; i < int(jobs.size()); i++)
  {
    if(jobs[i].jid == jobId)
    {
      return &jobs[i];
    }
  }
  
  return nullptr;
}

JobsList::JobEntry::JobEntry(jobid_t jid, pid_t pid, vector<string> args, string cmd_line, time_t start_time, bool is_stopped)
  : jid(jid), pid(pid), args(args), cmd_line(cmd_line), start_time(start_time), is_stopped(is_stopped) {} 


KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line)
{
  this->jobs =  jobs;
  this->jid = stoi(this->args[2]);
  this->signum = stoi(args[1]);

}

void KillCommand::execute()
{

  if(args.size() > 3 || to_string(jid) != this->args[2] || to_string(signum) != args[1]) //TODO: check formating of the -<signum> arg
  {
    _serror("kill: invalid arguments");
  }
  JobsList::JobEntry* job = jobs->getJobById(jid);
  
  if(!job)
  {
    _serror("kill: job-id <job-id> does not exist");
    return;
  }


  if(kill(job->pid,-1*signum) != 0)
  {
    _serrorSys("kill");
  }
}
/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
	// For example:

  string cmd_s = _trim(string(cmd_line));
  string firstWord = cmd_s.substr(0, cmd_s.find_first_of(WHITESPACE));

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
  else if (firstWord.compare("kill") == 0) {
    return new KillCommand(cmd_line, &(this->jobs));
  }
  else {
    return new ExternalCommand(cmd_line, &(this->jobs),&fg_pid);
  }
  
  return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
  // TODO: fix & striping.
  Command* cmd1 = nullptr;
  Command* cmd2 = nullptr;

  int stdin_copy = dup(0);
  int stdout_copy = dup(1);
  int stderr_copy = dup(2);

  
  string cmd_s = string(cmd_line);
  size_t pipe_index_s = cmd_s.find_first_of(">|");

  if(pipe_index_s != string::npos)
  {
    if(cmd_s.substr(pipe_index_s,1) == ">")
    {
      int file;
      if(cmd_s.substr(pipe_index_s,2) == ">>")
      {
        file = open(cmd_s.substr(pipe_index_s+2,cmd_s.size()-pipe_index_s+2).c_str(),O_CREAT|O_WRONLY|O_APPEND,0644);
        //CHECK MODE NUMBERS
      }
      else
      {
        file = open(cmd_s.substr(pipe_index_s+1,cmd_s.size()-pipe_index_s+1).c_str(),O_CREAT|O_WRONLY|O_TRUNC,0644);
        //CHECK MODE NUMBERS
      }
      
      if(file == -1)
      {
        _serrorSys("open");
        return;    
      }
      
      close(1);
      dup(file);
      char *cmd_line_noamp = new char[cmd_s.length()+1];
      strcpy(cmd_line_noamp, cmd_s.substr(0,pipe_index_s-1).c_str());
      _removeBackgroundSign(cmd_line_noamp);
      cmd1 = CreateCommand(cmd_line_noamp);
      if (cmd1)
      {
        cmd1->execute();
        delete cmd1;
      }
      delete cmd_line_noamp;
      dup2(stdout_copy,1);
      close(file);    
    }
    else if(cmd_s.substr(pipe_index_s,1) == "|")
    {
      int cmd_pipe[2];
      pipe(cmd_pipe);
      pid_t son = fork();
      if (son < 0)
      {
        _serrorSys("fork");
      }
      
      if(son == 0) // son to execute cmd2.
      {
        close(0);
        close(cmd_pipe[1]);
        dup(cmd_pipe[0]);
        char *cmd2_line_noamp = new char[cmd_s.length()+1];

        if (cmd_s.substr(pipe_index_s,2) != "|&")
        {
          strcpy(cmd2_line_noamp, cmd_s.substr(pipe_index_s+1,cmd_s.size()-pipe_index_s-1).c_str());
        }
        else
        {
          strcpy(cmd2_line_noamp, cmd_s.substr(pipe_index_s+2,cmd_s.size()-pipe_index_s-2).c_str());
        }
        _removeBackgroundSign(cmd2_line_noamp);
        cmd2 = CreateCommand(cmd2_line_noamp);
        if (cmd2)
        {
          cmd2->execute();
          delete cmd2;
        }
        delete cmd2_line_noamp;
        dup2(stdin_copy,0);
        close(cmd_pipe[0]);
        exit(0);
      }
      else
      {
        if (cmd_s.substr(pipe_index_s,2) == "|&")
        {
          close(2);
        }
        else
        {
          close(1);
        }
        close(cmd_pipe[0]);
        dup(cmd_pipe[1]);
        char *cmd1_line_noamp = new char[cmd_s.length()+1];
        strcpy(cmd1_line_noamp, cmd_s.substr(0,pipe_index_s-1).c_str());
        _removeBackgroundSign(cmd1_line_noamp);
        cmd1 = CreateCommand(cmd1_line_noamp);
        if (cmd1)
        {
          cmd1->execute();
          delete cmd1;
        }
        delete cmd1_line_noamp;
        close(cmd_pipe[1]);

        if (cmd_s.substr(pipe_index_s,2) == "|&")
        {
          dup2(stderr_copy,2);
        }
        else
        {
          dup2(stdout_copy,1);
        }
      }
      waitpid(son,NULL,0);
    }
  }
  else
  {
    cmd1 = CreateCommand(cmd_line);
    if (cmd1) {
      cmd1->execute();
      delete cmd1;
    }
  } 
  //Please note that you must fork smash process for some commands (e.g., external commands....)
}