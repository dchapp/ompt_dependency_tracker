#ifndef CALLBACKS_H
#define CALLBACKS_H

#include "Task.hpp"
#include <sys/time.h>

// For getting thread ID in callbacks 
typedef struct my_ompt_data{
  uint64_t own;
  ompt_data_t client;
} my_ompt_data_t;

uint64_t get_own_data(ompt_data_t* data)
{
  return ((my_ompt_data_t*)data->ptr)->own;
}

uint64_t alloc_and_init_own_data(ompt_data_t* data, uint64_t init_data)
{
  data->ptr = malloc(sizeof(my_ompt_data_t));
  ((my_ompt_data_t*)data->ptr)->own = init_data;
  return init_data; 
}

static uint64_t my_next_id() {
  static uint64_t ID=0;
  uint64_t ret = __sync_fetch_and_add(&ID, 1); 
  return ret; 
}

static uint64_t my_next_task_id() {
  static uint64_t ID = 0;
  uint64_t ret = __sync_fetch_and_add(&ID, 1);
  return ret; 
}

static void
on_ompt_callback_thread_begin(
  ompt_thread_type_t thread_type,
  ompt_data_t *thread_data)
{
  n_thread_begin++;
  uint64_t tid = alloc_and_init_own_data(thread_data, my_next_id());
#ifdef DEBUG
  printf("Begin thread %lu\n", tid);
#endif 
}

static void
on_ompt_callback_thread_end(
  ompt_data_t *thread_data)
{
  n_thread_end++; 
#ifdef DEBUG
  uint64_t tid = get_own_data(ompt_get_thread_data());
  printf("End thread %lu\n", tid);
#endif 
}

static void
on_ompt_callback_parallel_begin(
  ompt_data_t *encountering_task_data,
  const omp_frame_t *encountering_task_frame,
  ompt_data_t* parallel_data,
  uint32_t requested_team_size,
  ompt_invoker_t invoker,
  const void *codeptr_ra)
{
  n_parallel_begin++;
#ifdef DEBUG
  printf("\nENTERING PARALLEL_BEGIN\n"); 
  if(parallel_data->ptr) {
    printf("0: parallel_data initially not null\n");
  }
#endif 
}

static void
on_ompt_callback_parallel_end(
  ompt_data_t *parallel_data,
  ompt_data_t *encountering_task_data,
  ompt_invoker_t invoker,
  const void *codeptr_ra)
{
  n_parallel_end++; 
#ifdef DEBUG
  printf("\nENTERING PARALLEL_END\n"); 
#endif
}

static void
on_ompt_callback_task_create(
    ompt_data_t *encountering_task_data,
    const omp_frame_t *encountering_task_frame,
    ompt_data_t* new_task_data,
    int type,
    int has_dependences,
    const void *codeptr_ra)
{
  // Increment event counter
  n_explicit_task++; 

  // Bookkeeping
  if (new_task_data->ptr) {
    std::cout << "0: new_task_data initially not null" << std::endl; 
  }
  //new_task_data->value = ompt_get_unique_id();
  new_task_data->value = my_next_task_id(); ;
  char buffer[2048];
  format_task_type(type, buffer); 

  // Further bookkeeping if this is the initial task
  if (type & ompt_task_initial) {
    ompt_data_t * parallel_data;
    ompt_get_parallel_info(0, &parallel_data, NULL);
    if (parallel_data->ptr) {
      std::cout << "0: parallel_data initially not null" << std::endl; 
    }
    parallel_data->value = ompt_get_unique_id();
  }

  // Collect task data 
  uint64_t thread_id = get_own_data(ompt_get_thread_data()); 
  uint64_t parent_task_id = encountering_task_data ? encountering_task_data->value : 0;
  uint64_t task_id = new_task_data->value;

  // Construct and register task object 
  Task * task = new Task(task_id, parent_task_id, thread_id, TaskType::Explicit, codeptr_ra); 
  register_task(task, tool_data_ptr);
}

static void
on_ompt_callback_implicit_task(
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    unsigned int team_size,
    unsigned int thread_num)
{
  if (endpoint == ompt_scope_begin) {
    n_implicit_task_created++;
  }
  else if (endpoint == ompt_scope_end) {
    n_implicit_task_completed++;
  }
  else {
#ifdef DEBUG
    printf("Endpoint not equal to ompt_scope_begin or ompt_scope_end "
           "encountered in implicit_task callback\n"); 
#endif 
  } 
}

void handle_out_dependence(uint64_t task_id, 
                           unsigned int flag, 
                           void* address,
                           tool_data_t* tool_data)
{
  boost::lock_guard<boost::shared_mutex> in_deps_lock(tool_data->in_deps_mtx);
  boost::lock_guard<boost::shared_mutex> out_deps_lock(tool_data->out_deps_mtx);
  boost::lock_guard<boost::shared_mutex> inout_deps_lock(tool_data->inout_deps_mtx);

  boost::lock_guard<boost::shared_mutex> dependences_lock(tool_data->dependences_mtx);
  
  // Handle previously existing IN dependences
  auto in_dep_to_tasks = &(tool_data->in_dep_to_tasks);
  auto search_in_deps = in_dep_to_tasks->find( address );
  if ( search_in_deps != in_dep_to_tasks->end() ) {
    // Detect task dependences
    for ( auto prev_task_id : in_dep_to_tasks->at(address) ) {
      auto deps_ptr = &(tool_data->dependences);
      auto deps_search = deps_ptr->find(prev_task_id);
      if (deps_search != deps_ptr->end()) {
        deps_search->second.push_back( task_id );
      } else {
        std::vector<uint64_t> task_list = { task_id };
        std::pair<uint64_t, std::vector<uint64_t> > kvp = { prev_task_id, task_list };
        deps_ptr->insert(kvp);
      }
#ifdef DEBUG
      std::cout << "DEPENDENCY! Task " << task_id 
                << " depends on Task " << prev_task_id 
                << " because Task " << task_id 
                << " has data location " << address 
                << " as an OUT dependence, and Previous Task " << prev_task_id
                << " has " << address << " as an IN dependence."
                << std::endl;
#endif
    }
  }
  
  // Handle previously existing INOUT dependences
  auto inout_dep_to_tasks = &(tool_data->inout_dep_to_tasks);
  auto search_inout_deps = inout_dep_to_tasks->find( address );
  if ( search_inout_deps != inout_dep_to_tasks->end() ) {
    // Detect task dependences
    for ( auto prev_task_id : inout_dep_to_tasks->at(address) ) {
      auto deps_ptr = &(tool_data->dependences);
      auto deps_search = deps_ptr->find(prev_task_id);
      if (deps_search != deps_ptr->end()) {
        deps_search->second.push_back( task_id );
      } else {
        std::vector<uint64_t> task_list = { task_id };
        std::pair<uint64_t, std::vector<uint64_t> > kvp = { prev_task_id, task_list };
        deps_ptr->insert(kvp);
      }
#ifdef DEBUG
      std::cout << "DEPENDENCY! Task " << task_id 
                << " depends on Task " << prev_task_id 
                << " because Task " << task_id 
                << " has data location " << address 
                << " as an OUT dependence, and Previous Task " << prev_task_id
                << " has " << address << " as an INOUT dependence."
                << std::endl;
#endif
    }
  }

  // Handle previously existing OUT dependences
  auto out_dep_to_tasks = &(tool_data->out_dep_to_tasks);
  auto search_out_deps = out_dep_to_tasks->find( address );
  if ( search_out_deps != out_dep_to_tasks->end() ) {
    // Detect task dependences
    for ( auto prev_task_id : out_dep_to_tasks->at(address) ) {
      auto deps_ptr = &(tool_data->dependences);
      auto deps_search = deps_ptr->find(prev_task_id);
      if (deps_search != deps_ptr->end()) {
        deps_search->second.push_back( task_id );
      } else {
        std::vector<uint64_t> task_list = { task_id };
        std::pair<uint64_t, std::vector<uint64_t> > kvp = { prev_task_id, task_list };
        deps_ptr->insert(kvp);
      }
#ifdef DEBUG
      std::cout << "DEPENDENCY! Task " << task_id 
                << " depends on Task " << prev_task_id 
                << " because Task " << task_id 
                << " has data location " << address 
                << " as an OUT dependence, and Previous Task " << prev_task_id
                << " has " << address << " as an OUT dependence."
                << std::endl;
#endif
    }
    search_out_deps->second.push_back( task_id );
  } else {
    std::vector<uint64_t> task_list = { task_id };
    out_dep_to_tasks->insert( {address, task_list} );
  }

}

void handle_in_dependence(uint64_t task_id, 
                           unsigned int flag, 
                           void* address,
                           tool_data_t* tool_data)
{
  boost::lock_guard<boost::shared_mutex> in_deps_lock(tool_data->in_deps_mtx);
  boost::lock_guard<boost::shared_mutex> out_deps_lock(tool_data->out_deps_mtx);
  boost::lock_guard<boost::shared_mutex> inout_deps_lock(tool_data->inout_deps_mtx);
  
  boost::lock_guard<boost::shared_mutex> dependences_lock(tool_data->dependences_mtx);

  // Handle previously existing OUT dependences
  auto out_dep_to_tasks = &(tool_data->out_dep_to_tasks);
  auto search_out_deps = out_dep_to_tasks->find( address );
  if ( search_out_deps != out_dep_to_tasks->end() ) {
    // Detect task dependences
    for ( auto prev_task_id : out_dep_to_tasks->at(address) ) {
      auto deps_ptr = &(tool_data->dependences);
      auto deps_search = deps_ptr->find(prev_task_id);
      if (deps_search != deps_ptr->end()) {
        deps_search->second.push_back( task_id );
      } else {
        std::vector<uint64_t> task_list = { task_id };
        std::pair<uint64_t, std::vector<uint64_t> > kvp = { prev_task_id, task_list };
        deps_ptr->insert(kvp);
      }
#ifdef DEBUG
      std::cout << "DEPENDENCY! Task " << task_id 
                << " depends on Task " << prev_task_id 
                << " because Task " << task_id 
                << " has data location " << address 
                << " as an IN dependence, and Previous Task " << prev_task_id
                << " has " << address << " as an OUT dependence."
                << std::endl;
#endif
    }
  }
  
  // Handle previously existing INOUT dependences
  auto inout_dep_to_tasks = &(tool_data->inout_dep_to_tasks);
  auto search_inout_deps = inout_dep_to_tasks->find( address );
  if ( search_inout_deps != inout_dep_to_tasks->end() ) {
    // Detect task dependences
    for ( auto prev_task_id : inout_dep_to_tasks->at(address) ) {
      auto deps_ptr = &(tool_data->dependences);
      auto deps_search = deps_ptr->find(prev_task_id);
      if (deps_search != deps_ptr->end()) {
        deps_search->second.push_back( task_id );
      } else {
        std::vector<uint64_t> task_list = { task_id };
        std::pair<uint64_t, std::vector<uint64_t> > kvp = { prev_task_id, task_list };
        deps_ptr->insert(kvp);
      }
#ifdef DEBUG
      std::cout << "DEPENDENCY! Task " << task_id 
                << " depends on Task " << prev_task_id 
                << " because Task " << task_id 
                << " has data location " << address 
                << " as an IN dependence, and Previous Task " << prev_task_id
                << " has " << address << " as an INOUT dependence."
                << std::endl;
#endif
    }
  }

  // Update IN dependences map
  auto in_dep_to_tasks = &(tool_data->in_dep_to_tasks);
  auto search_in_deps = in_dep_to_tasks->find( address );
  if ( search_in_deps != in_dep_to_tasks->end() ) {
    search_in_deps->second.push_back( task_id );
  } else {
    std::vector<uint64_t> task_list = { task_id };
    in_dep_to_tasks->insert( {address, task_list} );
  }
}




static void
on_ompt_callback_task_dependences(
  ompt_data_t *task_data,
  const ompt_task_dependence_t *deps,
  int ndeps)
{
  // Increment event counter
  n_task_dependences++;

  // Look up task
  uint64_t task_id = task_data->value;

  auto deps_ptr = (ompt_task_dependence_t*) deps;
  unsigned int dep_flag;
  void * dep_address; 
  int dep_idx;
  for (dep_idx = 0; dep_idx < ndeps; dep_idx++) {
    dep_flag = deps_ptr->dependence_flags;
    dep_address = deps_ptr->variable_addr; 
    
    //printf("Task %lu, dependency %d: flag = %u, address = %p\n", task_id, dep_idx, dep_flag, dep_address);

    // OUT dependence
    if ( dep_flag == 1 ) {
    } 
    // IN dependence
    else if ( dep_flag == 2 ) {
      handle_in_dependence(task_id, dep_flag, dep_address, tool_data_ptr); 
    } 
    // INOUT dependence? (LLVM OpenMP is treating this one as OUT
    else if ( dep_flag == 3 ) {
      handle_out_dependence(task_id, dep_flag, dep_address, tool_data_ptr); 
    } 
    // Unrecognized/unsupported dependence type 
    else {
    }

    deps_ptr++; 
  }
}

static void
on_ompt_callback_task_dependence(
  ompt_data_t *first_task_data,
  ompt_data_t *second_task_data)
{
  n_task_dependence++; 
}

static void
on_ompt_callback_task_schedule(
    ompt_data_t *first_task_data,
    ompt_task_status_t prior_task_status,
    ompt_data_t *second_task_data)
{
  // First, determine task IDs and thread involved
  uint64_t first_task_id = first_task_data->value;
  uint64_t second_task_id = second_task_data->value;
  uint64_t thread_id = get_own_data(ompt_get_thread_data());
  TSP tsp; 
  tsp.thread_id = thread_id;
  struct timeval tv;
  struct timezone tz;
  gettimeofday(&tv, &tz); 
  tsp.time = (double)tv.tv_sec;

  // Got error when using "ompt_task_switch" instead of "4" (See pg. 440 of TR7) 
  // Using "ompt_task_others" works though (see pg. 400 of TR4)
  if (prior_task_status == ompt_task_others) { 
    n_task_schedule_others++;
    tsp.tsp_type = "other";
  }
  else if (prior_task_status == ompt_task_cancel) {
    n_task_schedule_cancel++;
    tsp.tsp_type = "cancel";
  } 
  else if (prior_task_status == ompt_task_yield) {
    n_task_schedule_yield++;
    tsp.tsp_type = "yield";
  }
  else if (prior_task_status == ompt_task_complete) {
    n_task_schedule_complete++;
    tsp.tsp_type = "complete";
  }
  else {
#ifdef DEBUG
    printf("\t- Prior task status unrecognized: %u\n", prior_task_status); 
#endif
  }

  //// Lookup task object and update its TSP sequence
  //auto id_to_task = &(tool_data_ptr->id_to_task);
  //auto task_search = id_to_task->find(first_task_id);
  //if (task_search != id_to_task->end()) {
  //  task_search->second->add_tsp(tsp);
  //}

}


void handle_taskwait_dependences(uint64_t task_id, tool_data_t* tool_data) {
  boost::lock_guard<boost::shared_mutex> t_lock(tool_data->id_to_task_mtx);
  boost::lock_guard<boost::shared_mutex> d_lock(tool_data->dependences_mtx);
  
  auto id_to_task = &(tool_data->id_to_task);
  auto dependences = &(tool_data->dependences);
  
  auto task_search = id_to_task->find(task_id);
#ifdef DEBUG
  printf("Handling taskwait dependences for task %lu\n", task_id);
#endif
  if (task_search != id_to_task->end()) {
    Task* t = task_search->second;
    for (auto child : t->get_children()) {
      uint64_t child_task_id = child->get_id();
#ifdef DEBUG
      printf("Has child task %lu\n", child_task_id);
#endif

      auto dep_search = dependences->find( child_task_id );
      if (dep_search != dependences->end()) {
        dep_search->second.push_back( task_id ); 
      } else {
        std::vector<uint64_t> dependent_task_list = { task_id };
        std::pair<uint64_t, std::vector<uint64_t> > kvp = { child_task_id, dependent_task_list };
        dependences->insert( kvp );
      }
    }
  }
}


static void
on_ompt_callback_sync_region(
  ompt_sync_region_kind_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  n_task_sync_region++;
  switch(endpoint)
  {
    case ompt_scope_begin:
      switch(kind)
      {
        case ompt_sync_region_barrier:
          n_task_sync_region_begin_barrier++;
          break;
        case ompt_sync_region_taskwait:
        {
          n_task_sync_region_begin_taskwait++;
          uint64_t task_id = task_data->value;
          uint64_t thread_id = get_own_data(ompt_get_thread_data()); 
          // Since we hit a taskwait, we know this task is dependent on its children
          handle_taskwait_dependences(task_id, tool_data_ptr); 
#ifdef DEBUG
          printf("Taskwait encountered. Task = %lu, Thread = %lu\n", task_id, thread_id);
#endif
          break;
        }
        case ompt_sync_region_taskgroup:
          n_task_sync_region_begin_taskgroup++;
          break; 
      }
      break;
    case ompt_scope_end:
      switch(kind) 
      {
        case ompt_sync_region_barrier:
          n_task_sync_region_end_barrier++;
          break;
        case ompt_sync_region_taskwait:
        {
          n_task_sync_region_end_taskwait++;
          uint64_t task_id = task_data->value;
          uint64_t thread_id = get_own_data(ompt_get_thread_data()); 
#ifdef DEBUG
          printf("Taskwait end. Task = %lu, Thread = %lu\n", task_id, thread_id);
#endif
          break;
        }
          break;
        case ompt_sync_region_taskgroup:
          n_task_sync_region_end_taskgroup++;
          break; 
      }
      break;
  }
}


static void
on_ompt_callback_sync_region_wait(
  ompt_sync_region_kind_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  n_task_sync_region_wait++;
  switch(endpoint)
  {
    case ompt_scope_begin:
      switch(kind)
      {
        case ompt_sync_region_barrier:
          n_task_sync_region_wait_begin_barrier++;
          break;
        case ompt_sync_region_taskwait:
          n_task_sync_region_wait_begin_taskwait++;
          break;
        case ompt_sync_region_taskgroup:
          n_task_sync_region_wait_begin_taskgroup++;
          break; 
      }
      break;
    case ompt_scope_end:
      switch(kind)
      {
        case ompt_sync_region_barrier:
          n_task_sync_region_wait_end_barrier++;
          break;
        case ompt_sync_region_taskwait:
          n_task_sync_region_wait_end_taskwait++;
          break;
        case ompt_sync_region_taskgroup:
          n_task_sync_region_wait_end_taskgroup++;
          break; 
      }
      break;
  }
}

#endif // CALLBACKS_H
