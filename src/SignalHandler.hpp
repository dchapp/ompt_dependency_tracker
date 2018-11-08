#ifndef SIGNAL_HANDLER_H
#define SIGNAL_HANDLER_H

#include <cstdio>
#include "BugLocalization.hpp" 
#include "DAGBuilder.hpp" 

void signal_handler_prologue(int signum) 
{
  // Print signal handler prologue
  printf("\n\nBug Localizer Tool caught signal: %d\n", signum);
  // Build dependency DAG
  printf("\t- Started building dependency DAG...\n");
  build_dag(tool_data_ptr);
  printf("\t- Finished building dependency DAG\n");
  printf("\t- Number of vertices in base dependency DAG: %lu", boost::num_vertices(tool_data_ptr->dag));
  printf("\t- Number of edges in base dependency DAG: %lu", boost::num_edges(tool_data_ptr->dag));
}

void signal_handler_epilogue(int signum)
{
  printf("Signal handler exiting\n");
  exit(signum);
}

void sigint_handler(int signum) 
{
  signal_handler_prologue(signum);
  signal_handler_epilogue(signum);
}

void sigfpe_handler(int signum) 
{
  signal_handler_prologue(signum);
  signal_handler_epilogue(signum);
}

void sigsegv_handler(int signum) 
{
  signal_handler_prologue(signum);
  signal_handler_epilogue(signum);
}

#endif // SIGNAL_HANDLER_H
