/* 
 * Copyright (C) 2014, Enrico M. Crisostomo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include "fsw.h"
#include "fsw_log.h"
#include <iostream>
#include <sstream>
#include <csignal>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cerrno>
#include <vector>
#include "poll_monitor.h"

#ifdef HAVE_GETOPT_LONG
#  include <getopt.h>
#endif
#ifdef HAVE_CORESERVICES_CORESERVICES_H
#  include "fsevent_monitor.h"
#endif
#ifdef HAVE_SYS_EVENT_H
#  include "kqueue_monitor.h"
#endif
#ifdef HAVE_SYS_INOTIFY_H
#  include "inotify_monitor.h"
#endif

using namespace std;

static const unsigned int TIME_FORMAT_BUFF_SIZE = 128;

static monitor *active_monitor = nullptr;
static vector<string> exclude_regex;
static vector<string> include_regex;
static bool _0flag = false;
static bool _1flag = false;
static bool Eflag = false;
static bool fflag = false;
static bool Iflag = false;
static bool kflag = false;
static bool lflag = false;
static bool Lflag = false;
static bool nflag = false;
static bool oflag = false;
static bool pflag = false;
static bool rflag = false;
static bool tflag = false;
static bool uflag = false;
static bool vflag = false;
static bool xflag = false;
static double lvalue = 1.0;
static string tformat = "%c";

bool is_verbose()
{
  return vflag;
}

static void usage(ostream& stream)
{
#ifdef HAVE_GETOPT_LONG
  stream << PACKAGE_STRING << "\n\n";
  stream << "Usage:\n";
  stream << PACKAGE_NAME << " [OPTION] ... path ...\n";
  stream << "\n";
  stream << "Options:\n";
  stream
    << " -0, --print0          Use the ASCII NUL character (0) as line separator.\n";
  stream
    << " -1, --one-event       Exit fsw after the first set of events is received.\n";
#  ifdef HAVE_REGCOMP
  stream << " -e, --exclude=REGEX   Exclude paths matching REGEX.\n";
  stream << " -E, --extended        Use extended regular expressions.\n";
#  endif
  stream
    << " -f, --format-time     Print the event time using the specified format.\n";
  stream << " -h, --help            Show this message.\n";
#  ifdef HAVE_REGCOMP
  stream << " -i, --include=REGEX   Include paths matching REGEX.\n";
  stream << " -I, --insensitive     Use case insensitive regular expressions.\n";
#  endif
#  if defined(HAVE_SYS_EVENT_H)
  stream << " -k, --kqueue          Use the kqueue monitor.\n";
#  endif
  stream << " -l, --latency=DOUBLE  Set the latency.\n";
  stream << " -L, --follow-links    Follow symbolic links.\n";
  stream << " -n, --numeric         Print a numeric event mask.\n";
  stream << " -o, --one-per-batch   Print a single message with the number of change events.\n";
  stream << "                       in the current batch.\n";
  stream << " -p, --poll            Use the poll monitor.\n";
  stream << " -r, --recursive       Recurse subdirectories.\n";
  stream << " -t, --timestamp       Print the event timestamp.\n";
  stream << " -u, --utc-time        Print the event time as UTC time.\n";
  stream << " -v, --verbose         Print verbose output.\n";
  stream << " -x, --event-flags     Print the event flags.\n";
  stream << "\n";
  stream << "See the man page for more information.";
  stream << endl;
#else
  string option_string = "[";
  option_string += "01";
#  ifdef HAVE_REGCOMP
  option_string += "eE";
#  endif
  option_string += "fh";
#  ifdef HAVE_REGCOMP
  option_string += "i";
#  endif
#  ifdef HAVE_SYS_EVENT_H
  option_string += "k";
#  endif
  option_string += "lLnoprtuvx";
  option_string += "]";

  stream << PACKAGE_STRING << "\n\n";
  stream << "Syntax:\n";
  stream << PACKAGE_NAME << " " << option_string << " path ...\n";
  stream << "\n";
  stream << "Usage:\n";
  stream << " -0  Use the ASCII NUL character (0) as line separator.\n";
  stream << " -1  Exit fsw after the first set of events is received.\n";
#  ifdef HAVE_REGCOMP
  stream << " -e  Exclude paths matching REGEX.\n";
  stream << " -E  Use extended regular expressions.\n";
#  endif
  stream << " -f  Print the event time stamp with the specified format.\n";
  stream << " -h  Show this message.\n";
  #  ifdef HAVE_REGCOMP
  stream << " -i  Use case insensitive regular expressions.\n";
  stream << " -i  Include paths matching REGEX.\n";
#  endif
#  ifdef HAVE_SYS_EVENT_H
  stream << " -k  Use the kqueue monitor.\n";
#  endif
  stream << " -l  Set the latency.\n";
  stream << " -L  Follow symbolic links.\n";
  stream << " -n  Print a numeric event masks.\n";
  stream << " -o  Print a single message with the number of change events in the current\n";
  stream << "     batch.\n";
  stream << " -p  Use the poll monitor.\n";
  stream << " -r  Recurse subdirectories.\n";
  stream << " -t  Print the event timestamp.\n";
  stream << " -u  Print the event time as UTC time.\n";
  stream << " -v  Print verbose output.\n";
  stream << " -x  Print the event flags.\n";
  stream << "\n";
  stream << "See the man page for more information.";
  stream << endl;
#endif
  exit(FSW_EXIT_USAGE);
}

static void close_stream()
{
  if (active_monitor)
  {
    delete active_monitor;

    active_monitor = nullptr;
  }
}

static void close_handler(int signal)
{
  close_stream();

  fsw_log("Done.\n");
  exit(FSW_EXIT_OK);
}

static bool validate_latency(double latency, ostream &ost, ostream &est)
{
  if (lvalue == 0.0)
  {
    est << "Invalid value: " << optarg << endl;
    return false;
  }

  if (errno == ERANGE || lvalue == HUGE_VAL)
  {
    est << "Value out of range: " << optarg << endl;
    return false;
  }

  if (is_verbose())
  {
    ost << "Latency set to: " << lvalue << endl;
  }

  return true;
}

static void register_signal_handlers()
{
  struct sigaction action;
  action.sa_handler = close_handler;
  sigemptyset(&action.sa_mask);
  action.sa_flags = 0;

  if (sigaction(SIGTERM, &action, nullptr) == 0)
  {
    fsw_log("SIGTERM handler registered.\n");
  }
  else
  {
    cerr << "SIGTERM handler registration failed." << endl;
  }

  if (sigaction(SIGABRT, &action, nullptr) == 0)
  {
    fsw_log("SIGABRT handler registered.\n");
  }
  else
  {
    cerr << "SIGABRT handler registration failed." << endl;
  }

  if (sigaction(SIGINT, &action, nullptr) == 0)
  {
    fsw_log("SIGINT handler registered.\n");
  }
  else
  {
    cerr << "SIGINT handler registration failed" << endl;
  }
}

static vector<string> decode_event_flag_name(vector<event_flag> flags)
{
  vector<string> names;

  for (event_flag flag : flags)
  {
    switch (flag)
    {
    case event_flag::PlatformSpecific:
      names.push_back("PlatformSpecific");
      break;
    case event_flag::Created:
      names.push_back("Created");
      break;
    case event_flag::Updated:
      names.push_back("Updated");
      break;
    case event_flag::Removed:
      names.push_back("Removed");
      break;
    case event_flag::Renamed:
      names.push_back("Renamed");
      break;
    case event_flag::OwnerModified:
      names.push_back("OwnerModified");
      break;
    case event_flag::AttributeModified:
      names.push_back("AttributeModified");
      break;
    case event_flag::IsFile:
      names.push_back("IsFile");
      break;
    case event_flag::IsDir:
      names.push_back("IsDir");
      break;
    case event_flag::IsSymLink:
      names.push_back("IsSymLink");
      break;
    case event_flag::Link:
      names.push_back("Link");
      break;
    default:
      names.push_back("<Unknown>");
      break;
    }
  }

  return names;
}

static void print_event_timestamp(const time_t &evt_time)
{
  char time_format_buffer[TIME_FORMAT_BUFF_SIZE];
  struct tm * tm_time = uflag ? gmtime(&evt_time) : localtime(&evt_time);

  string date =
    strftime(
             time_format_buffer,
             TIME_FORMAT_BUFF_SIZE,
             tformat.c_str(),
             tm_time) ? string(time_format_buffer) : string("<date format error>");

  cout << date << " ";
}

static void print_event_flags(const vector<event_flag> &flags)
{
  if (nflag)
  {
    int mask = 0;
    for (const event_flag &flag : flags)
    {
      mask += static_cast<int> (flag);
    }

    cout << " " << mask;
  }
  else
  {
    vector<string> flag_names = decode_event_flag_name(flags);

    for (string &name : flag_names)
    {
      cout << " " << name;
    }
  }
}

static void end_event_record()
{
  if (_0flag)
  {
    cout << '\0';
    cout.flush();
  }
  else
  {
    cout << endl;
  }
}

static void write_one_batch_event(const vector<event> &events)
{
  cout << events.size();
  end_event_record();
}

static void write_events(const vector<event> &events)
{
  for (const event &evt : events)
  {
    if (tflag) print_event_timestamp(evt.get_time());

    cout << evt.get_path();

    if (xflag)
    {
      print_event_flags(evt.get_flags());
    }

    end_event_record();
  }

  if (_1flag)
  {
    ::exit(FSW_EXIT_OK);
  }
}

static void process_events(const vector<event> &events)
{
  if (oflag)
    write_one_batch_event(events);
  else
    write_events(events);
}


static void start_monitor(int argc, char ** argv, int optind)
{
  // parsing paths
  vector<string> paths;

  for (auto i = optind; i < argc; ++i)
  {
    char *real_path = ::realpath(argv[i], nullptr);
    string path = (real_path ? real_path : argv[i]);

    fsw_log("Adding path: ");
    fsw_log(path.c_str());
    fsw_log("\n");

    paths.push_back(path);
  }

  if (pflag)
  {
    active_monitor = new poll_monitor(paths, process_events);
  }
  else if (kflag)
  {
#ifdef HAVE_SYS_EVENT_H
    active_monitor = new kqueue_monitor(paths, process_events);
#endif
  }
  else
  {
#if defined(HAVE_CORESERVICES_CORESERVICES_H)
    active_monitor = new fsevent_monitor(paths, process_events);
#elif defined(HAVE_SYS_EVENT_H)
    active_monitor = new kqueue_monitor(paths, process_events);
#elif defined(HAVE_SYS_INOTIFY_H)
    active_monitor = new inotify_monitor(paths, process_events);
#else
    active_monitor = new poll_monitor(paths, process_events);
#endif
  }

  active_monitor->set_latency(lvalue);
  active_monitor->set_recursive(rflag);
  active_monitor->set_exclude(exclude_regex, !Iflag, Eflag);
  active_monitor->set_include(include_regex, !Iflag, Eflag);
  active_monitor->set_follow_symlinks(Lflag);

  active_monitor->run();
}

static void parse_opts(int argc, char ** argv)
{
  int ch;
  ostringstream short_options;

  short_options << "01f:hkl:Lnoprtuvx";
#ifdef HAVE_REGCOMP
  short_options << "e:Ei:I";
#endif
#ifdef HAVE_SYS_EVENT_H
  short_options << "k";
#endif

#ifdef HAVE_GETOPT_LONG
  int option_index = 0;
  static struct option long_options[] = {
    { "print0", no_argument, nullptr, '0'},
    { "one-event", no_argument, nullptr, '1'},
#  ifdef HAVE_REGCOMP
    { "exclude", required_argument, nullptr, 'e'},
    { "extended", no_argument, nullptr, 'E'},
#  endif
    { "format-time", required_argument, nullptr, 'f'},
    { "help", no_argument, nullptr, 'h'},
#  ifdef HAVE_REGCOMP
    { "include", required_argument, nullptr, 'i'},
    { "insensitive", no_argument, nullptr, 'I'},
#  endif
#  ifdef HAVE_SYS_EVENT_H
    { "kqueue", no_argument, nullptr, 'k'},
#  endif
    { "latency", required_argument, nullptr, 'l'},
    { "follow-links", no_argument, nullptr, 'L'},
    { "numeric", no_argument, nullptr, 'n'},
    { "one-per-batch", no_argument, nullptr, 'o'},
    { "poll", no_argument, nullptr, 'p'},
    { "recursive", no_argument, nullptr, 'r'},
    { "timestamp", no_argument, nullptr, 't'},
    { "utc-time", no_argument, nullptr, 'u'},
    { "verbose", no_argument, nullptr, 'v'},
    { "event-flags", no_argument, nullptr, 'x'},
    { nullptr, 0, nullptr, 0}
  };

  while ((ch = getopt_long(
                           argc,
                           argv,
                           short_options.str().c_str(),
                           long_options,
                           &option_index)) != -1)
  {
#else
  while ((ch = getopt(argc, argv, short_options.str().c_str())) != -1)
  {
#endif

    switch (ch)
    {
    case '0':
      _0flag = true;
      break;

    case '1':
      _1flag = true;
      break;
      
#ifdef HAVE_REGCOMP
    case 'e':
      exclude_regex.push_back(optarg);
      break;

    case 'E':
      Eflag = true;
      break;
#endif

    case 'f':
      fflag = true;
      tformat = string(optarg);
      break;

    case 'h':
      usage(cout);
      exit(FSW_EXIT_USAGE);

#ifdef HAVE_REGCOMP
    case 'i':
      include_regex.push_back(optarg);
      break;
      
    case 'I':
      Iflag = true;
      break;
#endif

#ifdef HAVE_SYS_EVENT_H
    case 'k':
      kflag = true;
      break;
#endif

    case 'l':
      lflag = true;
      lvalue = strtod(optarg, nullptr);

      if (!validate_latency(lvalue, cout, cerr))
      {
        exit(FSW_EXIT_LATENCY);
      }

      break;

    case 'L':
      Lflag = true;
      break;

    case 'n':
      nflag = true;
      xflag = true;
      break;

    case 'o':
      oflag = true;
      break;

    case 'p':
      pflag = true;
      break;

    case 'r':
      rflag = true;
      break;

    case 't':
      tflag = true;
      break;

    case 'u':
      uflag = true;
      break;

    case 'v':
      vflag = true;
      break;

    case 'x':
      xflag = true;
      break;

    default:
      usage(cerr);
      exit(FSW_EXIT_UNK_OPT);
    }
  }
}

int main(int argc, char ** argv)
{
  parse_opts(argc, argv);

  // validate options
  if (optind == argc)
  {
    cerr << "Invalid number of arguments." << endl;
    ::exit(FSW_EXIT_UNK_OPT);
  }

  // only one kind of monitor can be used at a time
  if (pflag && kflag)
  {
    cerr << "-k and -p are mutually exclusive." << endl;
    ::exit(FSW_EXIT_OPT);
  }
  
  // configure and start the monitor
  try
  {
    // registering handlers to clean up resources
    register_signal_handlers();
    ::atexit(close_stream);

    // configure and start the monitor loop
    start_monitor(argc, argv, optind);
  }
  catch (exception & conf)
  {
    cerr << "An error occurred and the program will be terminated.\n";
    cerr << conf.what() << endl;

    return FSW_EXIT_ERROR;
  }
  catch (...)
  {
    cerr << "An unknown error occurred and the program will be terminated.\n";

    return FSW_EXIT_ERROR;
  }

  return FSW_EXIT_OK;
}
