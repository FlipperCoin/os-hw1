#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <queue>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define DEFAULT_PROMPT "smash"

const std::string WHITESPACE = " \n\r\t\f\v";
typedef int jobid_t;

using namespace std;

void _serror(string error);
void _serrorSys(string sysname);

class Command {
  public:
    string cmd_line;
    vector<string> args;
// TODO: Add your data members
 public:
  Command(const char* cmd_line);
  virtual ~Command();
  virtual void execute() = 0;
  //virtual void prepare();
  //virtual void cleanup();
  // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
 public:
  BuiltInCommand(const char* cmd_line) : Command(cmd_line) {};
  virtual ~BuiltInCommand() {}
};

class JobsList;
class ExternalCommand : public Command {
  protected:
    bool isBackgroundCommand;
    JobsList* jobs;
    pid_t* fg_pid;
    jobid_t* fg_jid;
    string* fg_cmd;
    char* createCmdStr();
  public:
    ExternalCommand(const char* cmd_line, JobsList* jobs, pid_t* fg_pid, jobid_t* fg_jid, string* fg_cmd);
    virtual ~ExternalCommand() {}
    void execute() override;
};

class PipeCommand : public Command {
  // TODO: Add your data members
 public:
  PipeCommand(const char* cmd_line);
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 // TODO: Add your data members
 public:
  explicit RedirectionCommand(const char* cmd_line);
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
  private:
    string* plast_pwd;
  public:
    ChangeDirCommand(const char* cmd_line, string* plast_pwd);
    virtual ~ChangeDirCommand() {}
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {};
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
  private:
    pid_t pid;
  public:
    ShowPidCommand(const char* cmd_line, pid_t pid);
    virtual ~ShowPidCommand();
    void execute() override;
};

class ChPromptCommand : public BuiltInCommand
{
  private:
    string prompt_name;
  public:
    ChPromptCommand(const char* cmd_line);
    virtual ~ChPromptCommand();
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
  private:
    JobsList* jobs;
  public:
// TODO: Add your data members public:
  QuitCommand(const char* cmd_line, JobsList* jobs);
  virtual ~QuitCommand() {}
  void execute() override;
};

class JobsList {
 public:
  class JobEntry {
    public:
      jobid_t jid;
      pid_t pid;
      string cmd_line;
      time_t start_time; // in seconds
      bool is_stopped;

      JobEntry(jobid_t jid, pid_t pid, string cmd_line, time_t start_time, bool is_stopped);
  };
  private:
    vector<JobEntry> jobs;
    //jobid_t next_jid;
    void printJobEntry(JobEntry& job);
  // TODO: Add your data members
  public:
    void clearZombieJobs();
    JobsList() = default;
    ~JobsList() = default;
    void addJob(string cmd_line, pid_t pid, jobid_t jid = 0, bool isStopped = false);
    void printJobsList();
    void killAllJobs();
    void removeFinishedJobs();
    JobEntry * getJobById(jobid_t jobId);
    void removeJobById(jobid_t jobId);
    JobEntry * getJobByPid(pid_t jobPid);
    JobEntry * getLastJob(jobid_t* lastJobId);
    JobEntry * getLastStoppedJob(jobid_t *jobId);
    int size();
    // jobid_t updateNextJid(); not needed rn
    // TODO: Add extra methods or modify exisitng ones as needed
};

class JobsCommand : public BuiltInCommand {
  private:
    JobsList* jobs;
 // TODO: Add your data members
 public:
  JobsCommand(const char* cmd_line, JobsList* jobs);
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 private:
  JobsList* jobs;
  jobid_t jid;
  int signum;
 public:
  KillCommand(const char* cmd_line, JobsList* jobs);
  virtual ~KillCommand() {}
  void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
 private:
  JobsList* jobs;
  pid_t* fg_pid;
  jobid_t* fg_jid;
  string* fg_cmd;
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs, pid_t* fg_pid, jobid_t* fg_jid, string* fg_cmd);
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 private:
  JobsList* jobs;
  pid_t* fg_pid;
  string* fg_cmd;
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs, pid_t* fg_pid, string* fg_cmd);
  virtual ~BackgroundCommand() {}
  void execute() override;
};

class HeadCommand : public BuiltInCommand {
 public:
  HeadCommand(const char* cmd_line);
  virtual ~HeadCommand() {}
  void execute() override;
};


class TimeOut
{
public:
  time_t timestamp;
  time_t duration;
  pid_t pid;
  time_t end_time;
  TimeOut(time_t timestamp, time_t duration,pid_t pid) : timestamp(timestamp), duration(duration), pid(pid), 
          end_time(timestamp + duration){};
  ~TimeOut() = default;
};

class cmp
{
  public:
    bool operator()(TimeOut a, TimeOut b) const;
};

class TimeOutCommand : public ExternalCommand
{
  private:
    priority_queue<TimeOut,vector<TimeOut>,cmp>* timed_list;
  public:
    TimeOutCommand(const char* cmd_line, JobsList* jobs, pid_t* fg_pid, jobid_t* fg_jid, string* fg_cmd , priority_queue<TimeOut,vector<TimeOut>,cmp>* timed_list);
    virtual ~TimeOutCommand();
    void execute() override;
};



class SmallShell {
 private:
  // TODO: Add your data members
  string prompt_name;
  string *plast_pwd;
  SmallShell();
 public:
  JobsList jobs;
  pid_t fg_pid;
  jobid_t fg_jid;
  string fg_cmd;
  pid_t pid;
  priority_queue<TimeOut,vector<TimeOut>,cmp> timed_list;
  Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  string getName();
  void setName(string prompt_name);
  // TODO: add extra methods as needed
};

#endif //SMASH_COMMAND_H_
