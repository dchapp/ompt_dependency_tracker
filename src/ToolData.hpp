#ifndef TOOL_DATA_H
#define TOOL_DATA_H

#include "boost/thread/mutex.hpp" 
#include "boost/thread/shared_mutex.hpp" 
#include "boost/thread/locks.hpp" 
#include <unordered_map> 
#include <inttypes.h> 

#include "tbb/tbb.h" 
#include "tbb/concurrent_unordered_map.h"

#include "Task.hpp" 
#include "ParallelRegion.hpp"
#include "DependencyDAG.hpp" 

/******************************************************************************\
\******************************************************************************/
typedef struct tool_data {
  boost::mutex global_mtx; 
  
  uint64_t initial_task_id; // TODO: Is this needed? 

  std::unordered_map<uint64_t, Task*> id_to_task; 
  boost::shared_mutex id_to_task_mtx; 
  std::unordered_map<uint64_t, ParallelRegion*> id_to_parallel_region;
  boost::shared_mutex id_to_parallel_region_mtx; 
 
  std::unordered_map< void*, std::vector<uint64_t> > in_dep_to_tasks;
  std::unordered_map< void*, std::vector<uint64_t> > out_dep_to_tasks;
  std::unordered_map< void*, std::vector<uint64_t> > inout_dep_to_tasks;
  boost::shared_mutex in_deps_mtx;
  boost::shared_mutex out_deps_mtx;
  boost::shared_mutex inout_deps_mtx;

  std::unordered_map<uint64_t, std::vector<uint64_t> > dependences; // TODO: fix in dependences and sync_region callbacks!!!!
  boost::shared_mutex dependences_mtx; 

  std::unordered_map<uint64_t, vertex_t> id_to_vertex;
  boost::shared_mutex id_to_vertex_mtx;
  //std::unordered_map< std::pair<uint64_t, uint64_t>, edge_t> id_pair_to_edge;
  boost::shared_mutex id_pair_to_edge_mtx;

  // The Dependency DAG itself
  // Don't add edges until shutdown or signal
  dag_t dag;
  boost::shared_mutex dag_mtx; 
} tool_data_t; 


void register_task_as_child_of_parallel_region(uint64_t region_id, 
                                               uint64_t task_id, 
                                               tool_data_t * tool_data) {
#ifdef COARSE_GRAINED_LOCKING
  boost::lock_guard<boost::shared_mutex> pmap_lock(tool_data->id_to_parallel_region_mtx);
  boost::lock_guard<boost::shared_mutex> tmap_lock(tool_data->id_to_task_mtx);
#endif 
  
  auto id_to_region = &(tool_data->id_to_parallel_region);
  auto id_to_task = &(tool_data->id_to_task);
  auto region_search = id_to_region->find(region_id);
  auto task_search = id_to_task->find(task_id);
  if (region_search != id_to_region->end() && task_search != id_to_task->end()) {
    region_search->second->add_child(task_search->second);    
  }
}

/* 
 *
 */
void register_parallel_region(ParallelRegion * new_region, tool_data_t * tool_data)
{
#ifdef COARSE_GRAINED_LOCKING
  boost::lock_guard<boost::shared_mutex> lock(tool_data->id_to_parallel_region_mtx);
#endif
  auto id_to_region = &(tool_data->id_to_parallel_region);
  uint64_t region_id = new_region->get_id();
  auto insert_result = id_to_region->insert({region_id, new_region});
#ifdef DEBUG
  if (insert_result.second == false) {
    printf("\t- Region %lu already has a ParallelRegion object associated with it\n",
           region_id);
  } else {
    printf("\t- Associating region %lu with ParallelRegion object:\n", region_id);
  }
#endif 
}


/* Associate a task ID to a pointer to the corresponding task object.
 * This is the only function that writes to the id_to_task map. 
 */
void register_task(Task * new_task, tool_data_t * tool_data) 
{
#ifdef COARSE_GRAINED_LOCKING
  boost::lock_guard<boost::shared_mutex> lock(tool_data->id_to_task_mtx);
#endif  
  // Insert task
  auto id_to_task = &(tool_data->id_to_task);
  uint64_t task_id = new_task->get_id();
  auto search = id_to_task->find(task_id);
  auto insert_result = id_to_task->insert( {task_id, new_task} );

  // Add task to parent's children
  auto parent_search = id_to_task->find(new_task->get_parent_id());
  if (parent_search != id_to_task->end()) {
    parent_search->second->add_child( new_task );
  }

#ifdef DEBUG
  if (insert_result.second == false) {
    printf("Task %lu already has a task object associated with it.\n",
           task_id);
  } else {
    printf("Task %lu successfully registered.\n", task_id);
    new_task->print(2);
  }
#endif 
}

/* Mark a task as complete */
void complete_task(uint64_t id, tool_data_t * tool_data) {
  std::unordered_map<uint64_t, Task*> * id_to_task = &(tool_data->id_to_task);
  auto search = id_to_task->find(id);
  if (search != id_to_task->end()) {
    search->second->change_state(TaskState::Completed); 
  } else {
#ifdef DEBUG
    printf("\t- ID %lu does not have a task object associated with it\n", id);
#endif
  }
}
#endif // TOOL_DATA_H
