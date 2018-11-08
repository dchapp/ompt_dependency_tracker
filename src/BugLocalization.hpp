#ifndef BUG_LOCALIZATION_H
#define BUG_LOCALIZATION_H


#include <iostream> 
#include "AddressTranslation.hpp"
#include "ToolData.hpp"
#include "Task.hpp" 

void localize_bugs();
void generate_report(const std::vector<uint64_t>& task_ids); 


void localize_bugs() 
{
  std::vector<uint64_t> flagged_tasks;
  // Dummy implementation treats all tasks as suspicious
  for (auto task : tool_data_ptr->id_to_task)
  {
    flagged_tasks.push_back(task.first);
  }
  generate_report(flagged_tasks);
}

void generate_report(const std::vector<uint64_t>& task_ids) 
{
#ifdef DEBUG
  printf("\nGenerating suspicious task report...\n");
#endif

  int n_tasks = task_ids.size();
  void* address_list[ n_tasks ];
  for ( int i = 0; i < n_tasks; i++ )
  {
    // Lookup task
    Task* task = tool_data_ptr->id_to_task.find(task_ids[i])->second; 
    // Get its codeptr_ra
    address_list[i] = (void*)task->get_codeptr_ra();

  }
  char** result = backtraceSymbols(address_list, n_tasks);
  for ( int i = 0; i < n_tasks; i++ ) 
  {
    std::cout << "Task: " << i 
              << ", codeptr_ra: " << address_list[i]
              << ", file(line)/function: " << result[i] << std::endl;
  }
}

#endif // BUG_LOCALIZATION_H
