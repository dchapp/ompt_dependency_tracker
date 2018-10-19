#include <iostream>
#include <vector> 
#include <unordered_map>
#include <csignal> 
#include <inttypes.h>
#include <stdio.h>

#include "omp.h"
#include "ompt.h"

/******************************************************************************\
 * Definitions for tool configuration
\******************************************************************************/

#define COARSE_GRAINED_LOCKING

//#define DEBUG
#define BUILD_DAG_ON_FINALIZE
//#define PRINT_SUMMARY_TASK_REGIONS 
//#define PRINT_SUMMARY_PARALLEL_REGIONS
//#define PRINT_EDGES 

#include "OMPT_helpers.hpp" 


/******************************************************************************\
 * Some counters for tracking how many callbacks of each kind are encountered
\******************************************************************************/
#ifdef ATOMIC_COUNTERS
  typedef std::atomic<uint64_t> counter_int;
#else
  typedef uint64_t counter_int; 
#endif
counter_int n_thread_begin;
counter_int n_thread_end;
counter_int n_parallel_begin;
counter_int n_parallel_end;
counter_int n_implicit_task_created;
counter_int n_implicit_task_completed;
counter_int n_explicit_task;
counter_int n_task_dependences;
counter_int n_task_dependence;
counter_int n_task_schedule_others;
counter_int n_task_schedule_cancel;
counter_int n_task_schedule_yield;
counter_int n_task_schedule_complete;
counter_int n_task_sync_region;
counter_int n_task_sync_region_begin_barrier;
counter_int n_task_sync_region_begin_taskwait;
counter_int n_task_sync_region_begin_taskgroup;
counter_int n_task_sync_region_end_barrier;
counter_int n_task_sync_region_end_taskwait;
counter_int n_task_sync_region_end_taskgroup;
counter_int n_task_sync_region_wait;
counter_int n_task_sync_region_wait_begin_barrier;
counter_int n_task_sync_region_wait_begin_taskwait;
counter_int n_task_sync_region_wait_begin_taskgroup;
counter_int n_task_sync_region_wait_end_barrier;
counter_int n_task_sync_region_wait_end_taskwait;
counter_int n_task_sync_region_wait_end_taskgroup;






#include "ToolData.hpp" 
#include "Task.hpp" 
#include "DependencyDAG.hpp"



static tool_data_t* tool_data_ptr;

#include "callbacks.hpp"

void add_parallel_region_vertex(ParallelRegion * pr) {
  // Synchronize access to the dag and the id_to_vertex map
  boost::lock_guard<boost::shared_mutex> dag_lock(tool_data_ptr->dag_mtx);
  boost::lock_guard<boost::shared_mutex> map_lock(tool_data_ptr->id_to_vertex_mtx);
  // First create vertex properties 
  const auto vp = construct_vprops(VertexType::ParallelRegion,
                                   pr->get_id(),
                                   pr->get_codeptr_ra());
  // Add a new vertex to the dag
  const vertex_t v = boost::add_vertex(vp, tool_data_ptr->dag);
  // Update the id_to_vertex map
  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
  auto res = id_to_vertex->insert( {pr->get_id(), v} ); 
}

vertex_t add_task_vertex(Task * t) {
  // Synchronize access to the dag and the id_to_vertex map
  //boost::lock_guard<boost::shared_mutex> dag_lock(tool_data_ptr->dag_mtx);
  //boost::lock_guard<boost::shared_mutex> map_lock(tool_data_ptr->id_to_vertex_mtx);
  // First create vertex properties 
  VertexType vt;
  if (t->get_type() == TaskType::Explicit) {
    vt = VertexType::ExplicitTask;
  } else {
    vt = VertexType::ImplicitTask;
  }
  const auto vp = construct_vprops(vt, t->get_id(), t->get_codeptr_ra());
  // Add a new vertex to the dag
  const vertex_t v = boost::add_vertex(vp, tool_data_ptr->dag);
  // Update the id_to_vertex map
  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
  auto res = id_to_vertex->insert( {t->get_id(), v} ); 
  return v; 
}


void add_vertices(tool_data_t * tool_data) {
  // Add vertices for parallel regions 
  auto id_to_region = tool_data->id_to_parallel_region;
  for (auto e : id_to_region) {
     add_parallel_region_vertex(e.second);   
  } 
  // Add vertices for tasks
  auto id_to_task = tool_data->id_to_task;
  for (auto e : id_to_task) {
     vertex_t v = add_task_vertex(e.second);   
  } 
}

void link_region_with_parent(ParallelRegion * pr) {
  uint64_t region_id = pr->get_id();
  uint64_t parent_id = pr->get_parent_id(); 
  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
  auto region_search = id_to_vertex->find(region_id);
  auto parent_search = id_to_vertex->find(parent_id);
  if (region_search != id_to_vertex->end() && parent_search != id_to_vertex->end()) {
    vertex_t region_vertex = region_search->second;
    vertex_t parent_vertex = parent_search->second;
    boost::add_edge(parent_vertex, region_vertex, tool_data_ptr->dag); 
  }
}

void link_task_with_parent(Task * t) {
  // Exit early if this is the initial task
  if (t->is_initial()) {
    return;
  } 
  uint64_t task_id = t->get_id();
  uint64_t parent_id = t->get_parent_id(); 
  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
  auto task_search = id_to_vertex->find(task_id);
  auto parent_search = id_to_vertex->find(parent_id);
  if (task_search != id_to_vertex->end() && parent_search != id_to_vertex->end()) {
    vertex_t task_vertex = task_search->second;
    vertex_t parent_vertex = parent_search->second;
    boost::add_edge(parent_vertex, task_vertex, tool_data_ptr->dag); 
  }
}


void add_edges(tool_data_t * tool_data) { 
  boost::lock_guard<boost::shared_mutex> dag_lock(tool_data_ptr->dag_mtx);
  boost::lock_guard<boost::shared_mutex> map_lock(tool_data_ptr->id_to_vertex_mtx);
  auto id_to_task = tool_data->id_to_task;
  auto dependences = tool_data->dependences;
  auto id_to_vertex = &(tool_data_ptr->id_to_vertex);
  for (auto e : dependences) {
    vertex_t src_v = id_to_vertex->find(e.first)->second;  
    for ( auto dependent_task : e.second ) {
      vertex_t dst_v = id_to_vertex->find( dependent_task )->second;
      boost::add_edge(src_v, dst_v, tool_data_ptr->dag);
    }
  } 
} 


void build_dag(tool_data_t * tool_data) {
  add_vertices(tool_data);
  add_edges(tool_data); 
}

void write_dag(dag_t dag) {
  printf("Writing DAG\n"); 
  // Get dotfile name for task tree visualization from environment
  char * env_var;
  std::string dag_dotfile;
  env_var = getenv("TASK_DAG_DOTFILE");
  if (env_var == NULL) {
    printf("TASK_DAG_DOTFILE not specified\n");
    dag_dotfile = "./dependency_dag.dot"; 
  } else {
    dag_dotfile = env_var;
  }
  // Open the stream to the dotfile for the task ancestry dag
  std::ofstream out(dag_dotfile);
  // Construct custom vertex writer
  auto dag_vw = make_vertex_writer(
    boost::get(&vertex_properties::vertex_id, dag) //,
    //boost::get(&vertex_properties::vertex_type, dag),
    //boost::get(&vertex_properties::color, dag),
    //boost::get(&vertex_properties::shape, dag),
    //boost::get(&vertex_properties::status, dag),
    //boost::get(&vertex_properties::codeptr_ra, dag),
    //boost::get(&vertex_properties::dependences, dag)
  );
  // Write out the task ancestry dag 
  boost::write_graphviz(out, dag, dag_vw); 

}

/******************************************************************************\
 * This function writes the parent-child tree and dependence DAG out to files
 * upon catching SIGINT or SIGSEGV 
\******************************************************************************/
void signal_handler(int signum) {

#ifdef DEBUG
  printf("\n\nTool caught signal: %d\n", signum);
#endif
  
  // Build the dag
  build_dag(tool_data_ptr);
  dag_t task_dag = tool_data_ptr->dag;

  std::cout << "Number of vertices in task dag: " << boost::num_edges(task_dag) << std::endl;

  // Write the dag
  write_dag(task_dag); 

  exit(signum);
}



int ompt_initialize(ompt_function_lookup_t lookup,
                    ompt_data_t * tool_data)
{
  // Instantiate the data structure that all the callbacks will refer to 
  tool_data->ptr = tool_data_ptr = new tool_data_t();

  // OMPT boilerplate 
  ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
  ompt_get_callback = (ompt_get_callback_t) lookup("ompt_get_callback");
  ompt_get_state = (ompt_get_state_t) lookup("ompt_get_state");
  ompt_get_task_info = (ompt_get_task_info_t) lookup("ompt_get_task_info");
  ompt_get_thread_data = (ompt_get_thread_data_t) lookup("ompt_get_thread_data");
  ompt_get_parallel_info = (ompt_get_parallel_info_t) lookup("ompt_get_parallel_info");
  ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id");
  ompt_get_num_procs = (ompt_get_num_procs_t) lookup("ompt_get_num_procs");
  ompt_get_num_places = (ompt_get_num_places_t) lookup("ompt_get_num_places");
  ompt_get_place_proc_ids = (ompt_get_place_proc_ids_t) lookup("ompt_get_place_proc_ids");
  ompt_get_place_num = (ompt_get_place_num_t) lookup("ompt_get_place_num");
  ompt_get_partition_place_nums = (ompt_get_partition_place_nums_t) lookup("ompt_get_partition_place_nums");
  ompt_get_proc_id = (ompt_get_proc_id_t) lookup("ompt_get_proc_id");
  ompt_enumerate_states = (ompt_enumerate_states_t) lookup("ompt_enumerate_states");
  ompt_enumerate_mutex_impls = (ompt_enumerate_mutex_impls_t) lookup("ompt_enumerate_mutex_impls");

  ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id"); 

  /* Set which callbacks this tool will enable */
  register_callback(ompt_callback_thread_begin);
  register_callback(ompt_callback_thread_end);
  
  register_callback(ompt_callback_parallel_begin);
  register_callback(ompt_callback_parallel_end);
  register_callback(ompt_callback_implicit_task);
  
  register_callback(ompt_callback_task_create);

  register_callback(ompt_callback_task_dependences);
  register_callback(ompt_callback_task_dependence);
  register_callback(ompt_callback_task_schedule);
  register_callback(ompt_callback_sync_region);
  register_callback_t(ompt_callback_sync_region_wait, ompt_callback_sync_region_t);
  

  // Register signal handlers for graph visualization 
  signal(SIGINT, signal_handler); 

#ifdef DEBUG
  printf("\n\n\n");
  printf("=================================================================\n");
  printf("====================== OMPT_INITIALIZE ==========================\n");
  printf("=================================================================\n");
  printf("\n\n\n");
#endif

  // Returning 1 instead of 0 lets the OpenMP runtime know to load the tool 
  return 1;
}


void ompt_finalize(ompt_data_t *tool_data)
{
#ifdef DEBUG
  printf("\n\n\n");
  printf("=================================================================\n");
  printf("======================= OMPT_FINALIZE ===========================\n");
  printf("=================================================================\n");
  printf("\n\n\n");

#ifdef PRINT_SUMMARY_TASK_REGIONS
  printf("Tasks:\n");
  for (auto e : tool_data_ptr->id_to_task) {
    e.second->print(2); 
    printf("\n"); 
  }
#endif

#ifdef PRINT_SUMMARY_PARALLEL_REGIONS
  printf("Parallel Regions:\n");
  for (auto e : tool_data_ptr->id_to_parallel_region) {
    e.second->print(2); 
    printf("\n"); 
  }
#endif
#endif 

#ifdef PRINT_EDGES
  for (auto e : tool_data_ptr->dependences) {
    printf("Tasks depending on task: %lu\n", e.first); 
    for (auto t : e.second) {
      printf("%lu --> %lu\n", e.first, t); 
    }
  } 
#endif 

#ifdef BUILD_DAG_ON_FINALIZE
  build_dag(tool_data_ptr);
  dag_t task_dag = tool_data_ptr->dag;
#ifdef DEBUG
  std::cout << "Number of vertices in task dag: " << boost::num_vertices(task_dag) << std::endl;
#endif 
  write_dag(task_dag); 
#endif

  // Delete tool data 
  tool_data_ptr->id_to_parallel_region.clear();
  tool_data_ptr->id_to_task.clear();
  tool_data_ptr->id_to_vertex.clear(); 
  delete (tool_data_t*)tool_data->ptr; 

#ifdef DEBUG
  printf("0: ompt_event_runtime_shutdown\n"); 
#endif
}



/* "A tool indicates its interest in using the OMPT interface by providing a 
 * non-NULL pointer to an ompt_fns_t structure to an OpenMP implementation as a 
 * return value from ompt_start_tool." (OMP TR4 4.2.1)
 */
ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version,
                                          const char *runtime_version)
{
  static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,
                                                            &ompt_finalize, 
                                                            0};
  return &ompt_start_tool_result;
}
