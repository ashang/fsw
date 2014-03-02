#include "poll_monitor.h"
#include "fsw_log.h"
#include "path_utils.h"
#include <unistd.h>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>

using namespace std;

poll_monitor::poll_monitor(vector<string> paths, EVENT_CALLBACK callback) :
    monitor(paths, callback)
{
  previous_data = new poll_monitor_data();
  new_data = new poll_monitor_data();
  time(&curr_time);
}

poll_monitor::~poll_monitor()
{
  delete previous_data;
  delete new_data;
}

bool poll_monitor::initial_scan_callback(
    const string &path,
    const struct stat &stat)
{
  if (previous_data->tracked_files.count(path))
    return false;

  watched_file_info wfi
  { stat.st_mtimespec.tv_sec, stat.st_ctimespec.tv_sec };
  previous_data->tracked_files[path] = wfi;

  return true;
}

bool poll_monitor::intermediate_scan_callback(
    const string &path,
    const struct stat &stat)
{
  if (new_data->tracked_files.count(path))
    return false;

  watched_file_info wfi
  { stat.st_mtimespec.tv_sec, stat.st_ctimespec.tv_sec };
  new_data->tracked_files[path] = wfi;

  if (previous_data->tracked_files.count(path))
  {
    watched_file_info pwfi = previous_data->tracked_files[path];
    vector<event_flag> flags;

    if (stat.st_mtimespec.tv_sec > pwfi.mtime)
    {
      flags.push_back(event_flag::Updated);
    }

    if (stat.st_ctimespec.tv_sec > pwfi.ctime)
    {
      flags.push_back(event_flag::AttributeModified);
    }

    if (flags.size() > 0)
    {
      events.push_back(
      { path, curr_time, flags });
    }

    previous_data->tracked_files.erase(path);
  }
  else
  {
    vector<event_flag> flags;
    flags.push_back(event_flag::Created);

    events.push_back(
    { path, curr_time, flags });
  }

  return true;
}

bool poll_monitor::add_path(
    const string &path,
    const struct stat &fd_stat,
    poll_monitor_scan_callback poll_callback)
{
  return ((*this).*(poll_callback))(path, fd_stat);
}

void poll_monitor::scan(const string &path, poll_monitor_scan_callback fn)
{
  if (!accept_path(path))
    return;

  struct stat fd_stat;
  if (!stat_path(path, fd_stat))
    return;

  if (follow_symlinks && S_ISLNK(fd_stat.st_mode))
  {
    string link_path;
    if (read_link_path(path, link_path))
      scan(link_path, fn);

    return;
  }
  else if (!add_path(path, fd_stat, fn))
    return;

  if (!recursive)
    return;

  if (!S_ISDIR(fd_stat.st_mode))
    return;

  vector<string> dirs_to_process;
  dirs_to_process.push_back(path);

  while (dirs_to_process.size() > 0)
  {
    const string current_dir = dirs_to_process.back();
    dirs_to_process.pop_back();

    vector<string> children;
    get_directory_children(current_dir, children);

    for (string &child : children)
    {
      if (child.compare(".") == 0 || child.compare("..") == 0)
        continue;

      const string fqpath = current_dir + "/" + child;

      if (!accept_path(path))
        continue;

      if (!stat_path(fqpath, fd_stat))
        continue;

      if (follow_symlinks && S_ISLNK(fd_stat.st_mode))
      {
        string link_path;
        if (read_link_path(fqpath, link_path))
        {
          scan(link_path, fn);
        }
        continue;
      }
      else if (!add_path(fqpath, fd_stat, fn))
      {
        continue;
      }

      if (S_ISDIR(fd_stat.st_mode))
      {
        dirs_to_process.push_back(fqpath);
      }
    }
  }
}

void poll_monitor::find_removed_files()
{
  vector<event_flag> flags;
  flags.push_back(event_flag::Removed);

  for (auto &removed : previous_data->tracked_files)
  {
    events.push_back(
    { removed.first, curr_time, flags });
  }
}

void poll_monitor::swap_data_containers()
{
  delete previous_data;
  previous_data = new_data;
  new_data = new poll_monitor_data();
}

void poll_monitor::collect_data()
{
  poll_monitor_scan_callback fn = &poll_monitor::intermediate_scan_callback;

  for (string &path : paths)
  {
    scan(path, fn);
  }

  find_removed_files();
  swap_data_containers();
}

void poll_monitor::collect_initial_data()
{
  poll_monitor_scan_callback fn = &poll_monitor::initial_scan_callback;

  for (string &path : paths)
  {
    scan(path, fn);
  }
}

void poll_monitor::notify_events()
{
  if (events.size())
  {
    callback(events);
    events.clear();
  }
}

void poll_monitor::run()
{
  collect_initial_data();

  while (true)
  {
#ifdef DEBUG
    fsw_log("Done scanning.\n");
#endif

    ::sleep(latency < MIN_POLL_LATENCY ? MIN_POLL_LATENCY : latency);

    time(&curr_time);

    collect_data();
    notify_events();
  }
}
